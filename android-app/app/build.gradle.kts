plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace   = "com.example.nukedsc55"
    compileSdk  = 36

    defaultConfig {
        applicationId   = "com.example.nukedsc55"
        minSdk          = 29       // Android 10 ??LG Velvet Í∏∞Ï???
        targetSdk       = 36
        versionCode     = 1
        versionName     = "1.0"

        // Í≤Ä?âÎêú NDK Î≤ÑÏ†Ñ
        ndkVersion = "28.2.13676358"

        ndk {
            abiFilters += listOf("arm64-v8a")   // LG Velvet = arm64 ?ÑÏö©
            // ?ÑÏöî ??"armeabi-v7a" Ï∂îÍ? Í∞Ä??
        }

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++20"
                arguments += listOf(
                    "-DANDROID_BUILD=1",
                    "-DANDROID_STL=c++_shared",
                    "-DANDROID_PLATFORM=android-29"
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path    = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled   = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
            isDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    kotlinOptions {
        jvmTarget = "1.8"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("com.github.mik3y:usb-serial-for-android:3.7.3")
}