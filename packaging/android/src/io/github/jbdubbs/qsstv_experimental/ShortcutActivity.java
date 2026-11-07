package io.github.jbdubbs.qsstv_experimental;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * What the launcher shortcuts (res/xml/shortcuts.xml) start. Android starts a static shortcut with "clear the task", which would finish the
 * running MainActivity and with it the receiver; this activity has a task of its own (taskAffinity ""), so only that one is cleared. It passes
 * the screen that was asked for on to MainActivity, which is brought to the front (or started, when the program is not running), and ends.
 */
public class ShortcutActivity extends Activity
{
    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        Intent main = new Intent(this, MainActivity.class);
        main.setAction(Intent.ACTION_MAIN);
        main.addCategory(Intent.CATEGORY_LAUNCHER);
        main.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        String screen = getIntent().getStringExtra("qsstv.screen");
        if (screen != null) main.putExtra("qsstv.screen", screen);
        startActivity(main);
        finish();
    }
}
