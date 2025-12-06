package blitz3d.runtime;

import org.libsdl.app.SDLActivity;

/**
 * Blitz3D-NG Game Activity
 *
 * This is the main entry point for Blitz3D games on Android.
 * It extends SDL2's SDLActivity to handle native code execution.
 */
public class GameActivity extends SDLActivity {

    /**
     * Returns the list of native libraries to load.
     * The "main" library contains the compiled Blitz3D game code.
     */
    @Override
    protected String[] getLibraries() {
        return new String[] {
            // SDL2 shared library
            "SDL2",
            // Your compiled game library
            "main"
        };
    }

    /**
     * Returns the main function name.
     * This is the entry point in the native library.
     */
    @Override
    protected String getMainFunction() {
        return "SDL_main";
    }

    /**
     * Returns arguments passed to the main function.
     * Override this to pass command-line arguments to your game.
     */
    @Override
    protected String[] getArguments() {
        return new String[0];
    }

    /**
     * Called when the activity is being destroyed.
     * Clean up any game resources here.
     */
    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
