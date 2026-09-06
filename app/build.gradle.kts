plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    // Uncomment ONLY after you add app/google-services.json from the Firebase console:
    // alias(libs.plugins.google.services)
}

android {
    namespace = "com.fintrack.dndbeginnerremote"
    compileSdk = 35

    // NDK r28+ aligns 64-bit .so files for 16 KB page-size devices by default
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.fintrack.dndbeginnerremote"
        minSdk = 30
        targetSdk = 35
        versionCode = 7
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                // 16 KB page-size support (required on newer Android / Play)
                arguments += listOf(
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
                )
                // Belt-and-suspenders linker flags for older cached toolchains
                arguments += listOf(
                    "-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384"
                )
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    packaging {
        jniLibs {
            // Uncompressed + page-aligned native libs (needed for 16 KB devices)
            useLegacyPackaging = false
            // Avoid duplicate native libs from Game Activity prefab
            pickFirsts += listOf("**/libc++_shared.so")
        }
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.games.activity)

    // Firebase is optional until google-services.json is present
    implementation(platform(libs.firebase.bom))
    implementation(libs.firebase.database)

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}
