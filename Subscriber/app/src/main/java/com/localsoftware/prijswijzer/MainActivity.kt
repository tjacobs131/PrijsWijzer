package com.localsoftware.prijswijzer

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.Text
import androidx.lifecycle.lifecycleScope
import com.onesignal.OneSignal
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            Text("Welcome to PrijsWijzer!")
        }

        // Prompt for push permission. fallbackToSettings = false does not send the user
        // to system settings if they previously denied. See the Mobile SDK reference.
        // In production, consider using an in-app message instead for better opt-in rates.
        lifecycleScope.launch {
            OneSignal.Notifications.requestPermission(false)
        }
    }
}