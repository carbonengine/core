// Copyright © 2025 CCP ehf.

package _Self

import _Self.buildTypes.*
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project
import jetbrains.buildServer.configs.kotlin.vcs.GitVcsRoot


object Project : Project({

    description = "Build / Publish pipeline for https://github.com/ccpgames/carbon-core"

    params {
        param("carbon_ref", "refs/heads/main")
        param("carbon-pipeline-tools-ref", "refs/heads/main")
    }
    
    subProject(Windows.Project)
    subProject(MacOS.Project)

    buildType(PublishToPerforce)
})
