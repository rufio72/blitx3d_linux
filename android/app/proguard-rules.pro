# Blitz3D-NG ProGuard Rules
# Optimized for Play Store release builds

# =====================================================
# GENERAL SETTINGS
# =====================================================

# Preserve line number information for debugging crash reports
-keepattributes SourceFile,LineNumberTable

# Hide original source file name if you prefer
# -renamesourcefileattribute SourceFile

# =====================================================
# SDL2 RULES
# =====================================================

# Keep SDL2 native interface
-keep class org.libsdl.app.** { *; }

# Keep SDL activity and all its native methods
-keepclassmembers class org.libsdl.app.SDLActivity {
    native <methods>;
    public static <methods>;
}

# =====================================================
# BLITZ3D RUNTIME RULES
# =====================================================

# Keep Blitz3D game activity
-keep class blitz3d.runtime.** { *; }

# Keep all native methods in Blitz3D classes
-keepclasseswithmembers class blitz3d.** {
    native <methods>;
}

# =====================================================
# JNI RULES
# =====================================================

# Keep classes that are called from native code
-keep class * implements java.lang.annotation.Annotation { *; }

# Don't warn about native methods
-dontwarn javax.annotation.**

# =====================================================
# ANDROID FRAMEWORK
# =====================================================

# Keep Parcelable implementations
-keepclassmembers class * implements android.os.Parcelable {
    public static final android.os.Parcelable$Creator CREATOR;
}

# Keep View constructors for layout inflation
-keepclassmembers class * extends android.view.View {
    public <init>(android.content.Context);
    public <init>(android.content.Context, android.util.AttributeSet);
    public <init>(android.content.Context, android.util.AttributeSet, int);
}

# =====================================================
# KOTLIN (if used)
# =====================================================

-dontwarn kotlin.**
-dontnote kotlin.**

# =====================================================
# OPTIMIZATION FLAGS
# =====================================================

# Remove logging in release builds
-assumenosideeffects class android.util.Log {
    public static boolean isLoggable(java.lang.String, int);
    public static int v(...);
    public static int i(...);
    public static int w(...);
    public static int d(...);
}

# Optimize aggressively
-optimizationpasses 5
-allowaccessmodification
-dontpreverify

# =====================================================
# DEBUGGING (comment out for release)
# =====================================================

# Uncomment to debug ProGuard issues:
# -printmapping mapping.txt
# -printseeds seeds.txt
# -printusage unused.txt
