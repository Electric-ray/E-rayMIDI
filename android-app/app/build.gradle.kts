plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace   = "com.example.nukedsc55"
    compileSdk  = 36

    defaultConfig {
        applicationId   = "com.example.nukedsc55"
        minSdk          = 29       // Android 10 — LG Velvet 기본값
        targetSdk       = 36
        versionCode     = 1
        versionName     = "1.0"

        // 검증된 NDK 버전
        ndkVersion = "28.2.13676358"

        ndk {
            abiFilters += listOf("arm64-v8a")   // LG Velvet = arm64 전용
            // 필요 시 "armeabi-v7a" 추가 가능
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

    // FluidSynth 공식 Android 프리빌트 .so들(app/src/main/cpp/fluidsynth/lib/<abi>/)을
    // 그대로 APK에 넣는다. CMake의 target_link_libraries(IMPORTED SHARED)만으로도
    // 최신 AGP는 대부분 자동 패키징하지만, 확실히 하기 위해 명시적으로도 지정.
    sourceSets {
        getByName("main") {
            jniLibs.srcDirs("src/main/cpp/fluidsynth/lib")
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
