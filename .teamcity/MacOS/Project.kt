// Copyright © 2025 CCP ehf.

package MacOS

import jetbrains.buildServer.configs.kotlin.Project
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.PullRequests
import jetbrains.buildServer.configs.kotlin.buildFeatures.XmlReport
import jetbrains.buildServer.configs.kotlin.buildFeatures.commitStatusPublisher
import jetbrains.buildServer.configs.kotlin.buildFeatures.freeDiskSpace
import jetbrains.buildServer.configs.kotlin.buildFeatures.nuGetFeedCredentials
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon
import jetbrains.buildServer.configs.kotlin.buildFeatures.pullRequests
import jetbrains.buildServer.configs.kotlin.buildFeatures.sshAgent
import jetbrains.buildServer.configs.kotlin.buildFeatures.xmlReport
import jetbrains.buildServer.configs.kotlin.buildSteps.exec
import jetbrains.buildServer.configs.kotlin.buildSteps.powerShell
import jetbrains.buildServer.configs.kotlin.buildSteps.python
import jetbrains.buildServer.configs.kotlin.buildSteps.script
import jetbrains.buildServer.configs.kotlin.triggers.vcs
import jetbrains.buildServer.configs.kotlin.vcs.GitVcsRoot
import jetbrains.buildServer.configs.kotlin.buildFeatures.provideAwsCredentials

val Debug = CarbonBuildMacOS("Debug MacOS", "Debug", "nmc-universal-osx-debug")
val Internal = CarbonBuildMacOS("Internal MacOS", "Internal", "nmc-universal-osx-internal")
val TrinityDev = CarbonBuildMacOS("TrinityDev MacOS", "TrinityDev", "nmc-universal-osx-trinitydev")
val Release = CarbonBuildMacOS("Release MacOS", "Release", "nmc-universal-osx-release")

object Project : Project({
    id("MacOS")
    name = "macOS"

    buildType(Debug)
    buildType(Internal)
    buildType(TrinityDev)
    buildType(Release)
})

class CarbonBuildMacOS(buildName: String, configType: String, preset: String) : BuildType({
    id(buildName.toId())
    name = buildName

    artifactRules = "%env.CMAKE_INSTALL_PREFIX%"

    params {
        param("env.SENTRY_CLI_DEBUG_SYMBOL_TYPE", "dsym")
        param("env.GIT_TAG_HASH_OVERRIDE", "")
        param("project", "eve-frontier")
        param("env.CMAKE_CONFIG_TYPE", configType)
        param("env.CMAKE_GENERATOR", "Ninja Multi-Config")
        param("teamcity.vcsTrigger.runBuildInNewEmptyBranch", "true")
        param("github_checkout_folder", "github")
        param("env.CTEST_JUNIT_OUTPUT_FILE", "ctest_results.xml")
        select("env.VISUAL_STUDIO_PLATFORM_TOOLSET", "v141", label = "Visual Studio Platform Toolset", description = "Specify the toolset for the build. e.g. v141 or v143.",
                options = listOf("v141 (2017)" to "v141", "v143 (2022)" to "v143"))
        param("env.CMAKE_BUILD_TARGETS", "all")
        param("env.CMAKE_INSTALL_PREFIX", ".build-artifact")
        param("env.SENTRY_PROJECT", "exefile-crashes")
        param("env.CMAKE_BUILD_FOLDER", ".cmake-build-%build.number%")
        param("env.GIT_TAG_HASH", "")
        param("env.EXECUTABLE_FILENAMES_MATCH", "")
        param("env.CMAKE_PRESET", preset)
        param("env.VCPKG_BINARY_SOURCES", "clear;x-aws,s3://vcpkg-binary-cache-static/cache/,readwrite")
        param("env.CMAKE_BUILD_PARALLEL_LEVEL", "8")
        param("env.CTEST_PARALLEL_LEVEL", "8")
    }


    vcs {
        root(AbsoluteId("Carbon_Core_2_CoreFeatureKotlin"),"+:. => %github_checkout_folder%")
        root(AbsoluteId("CarbonPipelineTools"), "+:. => carbon_pipeline_tools")
        cleanCheckout = true
    }

    steps {
        exec {
            name = "(macOS) Get Git Tag/Hash"
            workingDir = "%teamcity.build.checkoutDir%/%github_checkout_folder%"
            path = "python3"
            arguments = "%teamcity.build.checkoutDir%/carbon_pipeline_tools/carbon/GetGitTagAndOrHash.py"
        }
        exec {
            name = "(macOS) Remove build artifacts folder"
            path = "rm"
            arguments = "-rf %env.CMAKE_INSTALL_PREFIX%"
        }
        exec {
            name = "Configure"
            path = "cmake"
            arguments = "--preset %env.CMAKE_PRESET% -S %teamcity.build.checkoutDir%/%github_checkout_folder% -B %env.CMAKE_BUILD_FOLDER% -DBUILD_DOCUMENTATION=ON -DINSTALL_TO_MONOLITH=ON -DCMAKE_INSTALL_PREFIX=%env.CMAKE_INSTALL_PREFIX%"
        }
        exec {
            name = "Build"
            path = "cmake"
            arguments = "--build %env.CMAKE_BUILD_FOLDER% --config %env.CMAKE_CONFIG_TYPE% --target %env.CMAKE_BUILD_TARGETS%"
        }
        exec {
            name = "Run Tests"
            workingDir = "%env.CMAKE_BUILD_FOLDER%"
            path = "ctest"
            arguments = "-C %env.CMAKE_CONFIG_TYPE% -V --output-on-failure --output-junit %env.CTEST_JUNIT_OUTPUT_FILE%"
        }
        exec {
            name = "Package artifact"
            path = "cmake"
            arguments = "--install %env.CMAKE_BUILD_FOLDER% --config %env.CMAKE_CONFIG_TYPE%"
        }
        exec {
            name = "Upload symbols to sentry"
            path = "sentry-cli"
            arguments = "upload-dif --wait %env.CMAKE_BUILD_FOLDER%"
            param("script.content", """
                #!/usr/bin/env bash -eu
                filesWithSymbols = (
                  # insert your binary files with symbols here!
                )
            """.trimIndent())
        }
        script {
            name = "(Mac OS) TGZ Artifacts"
            scriptContent = """
                cd %env.CMAKE_INSTALL_PREFIX%
                tar -czvf artifacts.tar.gz *
            """.trimIndent()
        }
    }

    triggers {
        vcs {
            triggerRules = "+:root=${AbsoluteId("Carbon_Core_2_CoreFeatureKotlin").id}:."

            param("disabled", "true")
        }
    }

    features {
        pullRequests {
            vcsRootExtId = "${AbsoluteId("Carbon_Core_2_CoreFeatureKotlin")}"
            provider = github {
                authType = token {
                    token = "credentialsJSON:06ae89f1-d5f2-4c8d-a91a-9712c233ce06"
                }
               // Constrain PR triggers to compatible refs so as to avoid erroneous triggers
                filterTargetBranch = """
                    +:refs/heads/main
                    +:refs/heads/release/*.x
                    -:refs/heads/release/1.x
                """.trimIndent()
                filterAuthorRole = PullRequests.GitHubRoleFilter.MEMBER
            }
        }
        commitStatusPublisher {
            publisher = github {
                githubUrl = "https://api.github.com"
                authType = personalToken {
                    token = "credentialsJSON:a37ec416-c03b-4ac3-a92b-ecbd3a3d8c8e"
                }
            }
        }
        xmlReport {
            reportType = XmlReport.XmlReportType.JUNIT
            rules = "+:%env.CMAKE_BUILD_FOLDER%/%env.CTEST_JUNIT_OUTPUT_FILE%"
            verbose = true
        }
        perfmon {
        }
        freeDiskSpace {
            requiredSpace = "10gb"
            failBuild = true
        }
        sshAgent {
            teamcitySshKey = "ccpgames-evetech GitHub"
        }
        provideAwsCredentials {
            awsConnectionId = "Carbon_AwsVcpkgBinaryCacheServiceAccount"
        }
    }

    requirements {
        startsWith("teamcity.agent.jvm.os.name", "Mac OS X")
    }
})


