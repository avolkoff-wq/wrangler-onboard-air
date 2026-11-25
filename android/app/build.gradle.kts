plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}
android {
    namespace = "air.wrangler"
    compileSdk = 34
    defaultConfig {
        applicationId = "air.wrangler"
        minSdk = 31
        targetSdk = 34
        versionCode = 9
        versionName = "0.8.0"
    }
    buildTypes {
        // installed as a file, not from a store: the debug key signs it
        getByName("release") { isMinifyEnabled = false; signingConfig = signingConfigs.getByName("debug") }
    }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
}
