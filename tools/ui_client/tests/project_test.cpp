// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: project model/validation.
#include <gtest/gtest.h>

#include <algorithm>

#include "fs_util.h"
#include "project.h"
#include "test_util.h"

using namespace rpe::ui;
using namespace rpe::ui::test;

namespace {

Project sampleProject()
{
    Project project = makeProjectTemplate("demo", "0.2.1", "/opt/retro-sdk", Platform::Pc,
                                          "hello_triangle", "hello_triangle");
    AssetEntry mesh;
    mesh.source = "assets/source/cube.gltf";
    mesh.output = "assets/build/cube.mesh";
    mesh.type = AssetType::Mesh;
    mesh.vertex_count = 8;
    mesh.triangle_count = 12;
    mesh.texture_id = 1;
    project.assets.push_back(mesh);
    AssetEntry texture;
    texture.source = "assets/source/texture.png";
    texture.output = "assets/build/texture.tex";
    texture.type = AssetType::Texture;
    texture.width = 256;
    texture.height = 256;
    project.assets.push_back(texture);
    return project;
}

bool hasError(const std::vector<ValidationIssue>& issues)
{
    return std::any_of(issues.begin(), issues.end(), [](const ValidationIssue& issue) {
        return issue.severity == IssueSeverity::Error;
    });
}

} // namespace

TEST(ProjectTest, RoundTrip)
{
    TempDir dir;
    const Project project = sampleProject();
    std::string error;
    ASSERT_TRUE(saveProject(dir.str(), project, &error, Lang::Ru)) << error;

    const ProjectLoadResult loaded = loadProject(dir.str(), Lang::Ru);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(loaded.project.name, project.name);
    EXPECT_EQ(loaded.project.engine_version, project.engine_version);
    EXPECT_EQ(loaded.project.sdk_path, project.sdk_path);
    EXPECT_EQ(loaded.project.platform, project.platform);
    EXPECT_EQ(loaded.project.target, project.target);
    ASSERT_EQ(loaded.project.assets.size(), 2u);
    EXPECT_EQ(loaded.project.assets[0].output, "assets/build/cube.mesh");
    EXPECT_EQ(loaded.project.assets[0].vertex_count, 8u);
    EXPECT_EQ(loaded.project.assets[1].width, 256u);
}

TEST(ProjectTest, UnknownFieldIsDeterministicError)
{
    TempDir dir;
    ASSERT_TRUE(saveProject(dir.str(), sampleProject(), nullptr, Lang::Ru));
    const std::string file = projectFilePath(dir.str());
    std::string text = readFile(file);
    text.insert(text.find('{') + 1, "\"unexpected\": 1,\n");
    writeFile(file, text);

    const ProjectLoadResult loaded = loadProject(dir.str(), Lang::Ru);
    EXPECT_FALSE(loaded.ok);
    EXPECT_NE(loaded.error.find("unexpected"), std::string::npos);
}

TEST(ProjectTest, UnsafePathsRejected)
{
    Project project = sampleProject();
    project.assets[0].source = "../outside.gltf";
    const std::vector<ValidationIssue> issues = validateProject(project, ".", false, Lang::Ru);
    EXPECT_TRUE(hasError(issues));
}

TEST(ProjectTest, UnsafeBuildDirRejected)
{
    Project project = sampleProject();
    project.build_dir = "/tmp/build";
    const std::vector<ValidationIssue> issues = validateProject(project, ".", false, Lang::Ru);
    EXPECT_TRUE(hasError(issues));
}

TEST(ProjectTest, InvalidVersionAndPlatform)
{
    Project project = sampleProject();
    project.engine_version = "0.2";
    EXPECT_TRUE(hasError(validateProject(project, ".", false, Lang::Ru)));

    project = sampleProject();
    project.target = "../evil";
    EXPECT_TRUE(hasError(validateProject(project, ".", false, Lang::Ru)));
}

TEST(ProjectTest, DuplicateAssetOutputRejected)
{
    Project project = sampleProject();
    project.assets[1].output = project.assets[0].output;
    project.assets[1].type = AssetType::Mesh;
    const std::vector<ValidationIssue> issues = validateProject(project, ".", false, Lang::Ru);
    EXPECT_TRUE(hasError(issues));
}

TEST(ProjectTest, AssetTypeMustMatchOutputExtension)
{
    Project project = sampleProject();
    project.assets[0].output = "assets/build/cube.tex";
    EXPECT_TRUE(hasError(validateProject(project, ".", false, Lang::Ru)));
}

TEST(ProjectTest, MissingPathsAreWarningsWhenChecked)
{
    TempDir dir;
    Project project = sampleProject();
    project.sdk_path = dir.str();
    const std::vector<ValidationIssue> issues = validateProject(project, dir.str(), true, Lang::Ru);
    EXPECT_FALSE(hasError(issues));
    EXPECT_FALSE(issues.empty());
}

TEST(ProjectTest, ProjectRootAndPaths)
{
    EXPECT_EQ(projectFilePath("/tmp/demo"), "/tmp/demo/.rpeproject");
    EXPECT_EQ(projectRoot("/tmp/demo/.rpeproject"), "/tmp/demo");
    Project project = sampleProject();
    EXPECT_EQ(resolveAssetOutput("/tmp/demo", project.assets[0]),
              "/tmp/demo/assets/build/cube.mesh");
    EXPECT_EQ(runTargetName(project), "hello_triangle");
}
