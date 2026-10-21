package io.github.bownz.dk64recomp;

import android.app.Presentation;
import android.content.Context;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Display;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;

/**
 * Content for a second display, such as the lower screen of a dual-screen handheld (AYN Thor):
 * DK64 action buttons ({@link ActionPadView}). The game keeps the main display to itself.
 *
 * A Presentation is a window that belongs to the activity, so it is only shown while the
 * activity is in the foreground; DK64Activity creates and dismisses it with the lifecycle.
 */
final class SecondScreenPresentation extends Presentation {
    private ActionPadView actionPad;

    SecondScreenPresentation(Context outerContext, Display display) {
        super(outerContext, display);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (getWindow() != null) {
            // Keep the second screen awake while the game runs, like the main one.
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            // Never take input focus from the game window: SDL would report a focus loss to
            // the game, and key/gamepad events would come here. Touches still arrive.
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
        }

        FrameLayout root = new FrameLayout(getContext());
        root.setBackgroundColor(Color.rgb(18, 18, 22));
        actionPad = new ActionPadView(getContext());
        root.addView(actionPad, new FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(root);

        actionPad.setActive(true);
    }

    ActionPadView getActionPad() {
        return actionPad;
    }

    @Override
    protected void onStop() {
        if (actionPad != null) {
            actionPad.setActive(false);
        }
        super.onStop();
    }
}
