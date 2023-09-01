/**
 * @file MainActivity.kt
 * @brief
 * @author xhunmon
 * @last_editor lq
 */

package com.example.rtmp

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.example.rtmp.core.Pusher

class MainActivity : AppCompatActivity() {
    private var pusher: Pusher? = null

    // 这里改推流地址
    private val rtmpUrl = "rtmp://222.192.6.64:1935/stream_phone"
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        if (requestPermission()) {
            init()
        }
    }

    private fun init() {
        pusher = Pusher.Config(this, findViewById(R.id.surfaceView), rtmpUrl).build()
    }

    private fun requestPermission(): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return true
        }
        if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED && checkSelfPermission(
                Manifest.permission.WRITE_EXTERNAL_STORAGE
            ) == PackageManager.PERMISSION_GRANTED
        ) {
            return true
        }
        requestPermissions(
            arrayOf(
                Manifest.permission.RECORD_AUDIO,
                Manifest.permission.CAMERA,
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.READ_EXTERNAL_STORAGE
            ), PERMISSION_REQUEST_CAMERA
        )
        return false
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == PERMISSION_REQUEST_CAMERA) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                init()
            } else {
                Toast.makeText(this@MainActivity, "申请相机权限失败！", Toast.LENGTH_SHORT).show()
            }
        }
    }

    fun preview(view: View?) {
        if (!pusher!!.preview()) {
            Toast.makeText(this, "在预览中……", Toast.LENGTH_SHORT).show()
        }
    }

    fun push(view: View?) {
        if (!pusher!!.push()) {
            Toast.makeText(this, "在推送中……", Toast.LENGTH_SHORT).show()
        }
    }

    fun stop(view: View?) {
        if (pusher!!.stop()) {
            Toast.makeText(this, "画面和推送已暂停成功！", Toast.LENGTH_SHORT).show()
        }
    }

    fun switchCamera(view: View?) {
        pusher!!.switchCamera()
    }

    fun write(view: View?) {
//        pusher.write();
    }

    companion object {
        private const val PERMISSION_REQUEST_CAMERA = 0x01
    }
}