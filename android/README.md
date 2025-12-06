# Blitz3D-NG Android Build

This directory contains the Android project configuration for building Blitz3D-NG games for Android devices and Google Play Store.

## Requirements

- Android Studio Arctic Fox (2020.3.1) or newer
- Android SDK 34 (Android 14)
- Android NDK r26b (26.1.10909125)
- JDK 17+
- Gradle 8.5+

## Quick Start

### 1. Setup

```bash
# Copy local properties template
cp local.properties.template local.properties

# Edit local.properties with your SDK path
# sdk.dir=/path/to/Android/Sdk
```

### 2. Build Debug APK

```bash
./gradlew assembleDebug
```

Output: `app/build/outputs/apk/debug/app-debug.apk`

### 3. Build Release APK

First, configure signing in `local.properties`:

```properties
RELEASE_STORE_FILE=/path/to/your.keystore
RELEASE_STORE_PASSWORD=your_password
RELEASE_KEY_ALIAS=your_alias
RELEASE_KEY_PASSWORD=your_password
```

Then build:

```bash
./gradlew assembleRelease
```

Output: `app/build/outputs/apk/release/app-release.apk`

### 4. Build App Bundle (for Play Store)

```bash
./gradlew bundleRelease
```

Output: `app/build/outputs/bundle/release/app-release.aab`

## Configuration

### App Settings

Edit `gradle.properties` to customize:

```properties
APP_ID=com.yourcompany.yourgame
APP_NAME=Your Game Name
VERSION_CODE=1
VERSION_NAME=1.0.0
```

### Signing

**Option 1: local.properties (recommended)**
```properties
RELEASE_STORE_FILE=/path/to/release.keystore
RELEASE_STORE_PASSWORD=password
RELEASE_KEY_ALIAS=alias
RELEASE_KEY_PASSWORD=password
```

**Option 2: Environment variables**
```bash
export BLITZ3D_KEYSTORE_FILE=/path/to/release.keystore
export BLITZ3D_KEYSTORE_PASSWORD=password
export BLITZ3D_KEY_ALIAS=alias
export BLITZ3D_KEY_PASSWORD=password
```

### Creating a Release Keystore

```bash
keytool -genkey -v \
  -keystore release.keystore \
  -alias my_key \
  -keyalg RSA \
  -keysize 2048 \
  -validity 10000
```

**IMPORTANT:** Back up your keystore and passwords! If lost, you cannot update your app on Play Store.

## Play Store Checklist

Before uploading to Play Store:

- [ ] Target SDK is 34+ (configured in `app/build.gradle`)
- [ ] Version code incremented (`gradle.properties`)
- [ ] Release signing configured
- [ ] App bundle (.aab) built
- [ ] App icons provided (all mipmap densities)
- [ ] Privacy policy URL ready
- [ ] Content rating questionnaire completed
- [ ] Store listing prepared (screenshots, description)

## Useful Commands

```bash
# Check signing configuration
./gradlew checkSigningConfig

# Print build info
./gradlew printBuildInfo

# Clean build
./gradlew clean

# Install debug APK
./gradlew installDebug

# Run tests
./gradlew test
```

## Troubleshooting

### "SDK location not found"
Create `local.properties` with your SDK path:
```properties
sdk.dir=/path/to/Android/Sdk
```

### "NDK not configured"
Either install NDK r26b via Android Studio SDK Manager, or specify path:
```properties
ndk.dir=/path/to/ndk/26.1.10909125
```

### "Release signing not configured"
The build will use debug signing if release is not configured. For Play Store, you must configure release signing.

### Build fails with CMake errors
Ensure your CMakeLists.txt paths are correct and SDL2 source is available.

## Architecture

```
android/
├── app/
│   ├── build.gradle           # App-level build config
│   ├── proguard-rules.pro     # Code optimization rules
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── cpp/               # Native C++ code
│       │   ├── CMakeLists.txt
│       │   └── main.cpp       # Entry point
│       ├── java/blitz3d/runtime/
│       │   └── GameActivity.java
│       └── res/               # Android resources
├── build.gradle               # Project-level build config
├── gradle.properties          # Build properties
├── settings.gradle            # Gradle settings
└── local.properties           # Local config (gitignored)
```
