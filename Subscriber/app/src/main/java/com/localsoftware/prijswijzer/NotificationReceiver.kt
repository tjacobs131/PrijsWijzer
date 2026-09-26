package com.localsoftware.prijswijzer

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

class NotificationReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        // goAsync() tells Android to keep the Receiver alive for a few seconds
        // while we fetch the network data on a background thread.
        val pendingResult = goAsync()

        CoroutineScope(Dispatchers.IO).launch {
            try {
                fetchAndShowNotification(context)
                Log.println(Log.DEBUG, "NotificationReceiver", "Notification received")
            } catch (e: Exception) {
                e.printStackTrace()
            } finally {
                pendingResult.finish() // Let the device go back to sleep
            }
        }
    }

    private suspend fun fetchAndShowNotification(context: Context) {
        Log.println(Log.DEBUG, "NotificationReceiver", "Fetching notification")

        val url = URL("http://192.168.2.15:8080/")
        val connection = withContext(Dispatchers.IO) {
            url.openConnection() as HttpURLConnection
        }

        connection.connectTimeout = 5000
        connection.readTimeout = 5000
        connection.requestMethod = "GET"

        if (connection.responseCode == 200) {
            Log.println(Log.DEBUG, "NotificationReceiver", "Response code is 200")

            val response = connection.inputStream.bufferedReader().use { it.readText() }

            val jsonObject = JSONObject(response)
            val payload = NotificationPayload(
                title = jsonObject.getString("title"),
                message = jsonObject.getString("message"),
                id = jsonObject.optInt("id", (1..1000).random())
            )

            Log.println(Log.DEBUG, "NotificationReceiver", "Notification payload: $payload")

            showNotification(context, payload)
        } else if (connection.responseCode == 404) {
            Log.println(Log.DEBUG, "NotificationReceiver", "No notifications found")
        }
        connection.disconnect()
    }

    private fun showNotification(context: Context, payload: NotificationPayload) {
        val channelId = "local_polling_notifications"
        val notificationManager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                channelId, "Price Alerts", NotificationManager.IMPORTANCE_HIGH
            )
            notificationManager.createNotificationChannel(channel)
        }

        val intent = Intent(context, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            context, 0, intent, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )

        val notification = NotificationCompat.Builder(context, channelId)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentTitle(payload.title)
            .setContentText(payload.message)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setContentIntent(pendingIntent)
            .setAutoCancel(true)
            .build()

        notificationManager.notify(payload.id, notification)
    }
}