pluginManagement {
    repositories {
        maven { url = uri("https://dl.google.com/dl/android/maven2/") }
        maven { url = uri("https://repo1.maven.org/maven2/") }
        gradlePluginPortal()
    }
}
plugins {
    id("org.gradle.toolchains.foojay-resolver-convention") version "0.10.0"
}

dependencyResolutionManagement {
    @Suppress("UnstableApiUsage")
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "grotto-android"
include(":app")
