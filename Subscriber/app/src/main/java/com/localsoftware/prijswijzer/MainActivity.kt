package com.localsoftware.prijswijzer

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.os.SystemClock
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.Text

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        Log.println(Log.DEBUG, "MainActivity", "onCreate")

        setContent {
            Text("Welcome to PrijsWijzer! Background polling enabled.")
        }

        scheduleBackgroundPolling(this)
    }

    private fun scheduleBackgroundPolling(context: Context) {
        val alarmManager = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
        val intent = Intent(context, NotificationReceiver::class.java)

        Log.println(Log.DEBUG, "NotificationReceiver", "Scheduling background polling")

        val pendingIntent = PendingIntent.getBroadcast(
            context,
            0,
            intent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )

        // Wake up every 15 minutes
        val intervalMillis: Long = 15 * (60 * 1000)
        Log.println(Log.DEBUG, "NotificationReceiver", "Scheduling background polling every $intervalMillis milliseconds")

        alarmManager.setInexactRepeating(
            AlarmManager.ELAPSED_REALTIME_WAKEUP,
            SystemClock.elapsedRealtime() + 1000, // First check in 1 second`
            intervalMillis,
            pendingIntent
        )
    }
}