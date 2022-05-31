package cn.zybwz.dwgame

import android.Manifest
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.TextView
import android.widget.Toast
import com.jiayz.dwgame.R
import com.permissionx.guolindev.PermissionX

class MainActivity : AppCompatActivity() {
    private lateinit var dwView: DWView
    val openSLRecorder = OpenSLRecorder()
    private lateinit var tip:TextView
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        dwView=findViewById(R.id.dwview)
        tip=findViewById(R.id.tip)
        PermissionX.init(this)
            .permissions(Manifest.permission.RECORD_AUDIO)
            .request { allGranted, grantedList, deniedList ->
                if (allGranted) {
                    openSLRecorder.init()
                } else {
                    Toast.makeText(this, "These permissions are denied: $deniedList", Toast.LENGTH_LONG).show()
                }
            }
            dwView.result=object :DWView.Result{
                override fun onFinish(count: Int) {
                    runOnUiThread{
                        when(count){
                            in 0..10->{
                                tip.text="亲建议您还是别出声了"
                            }
                            in 10..20->{
                                tip.text="有点东西，但东西不多"
                            }
                            in 20..25->{
                                tip.text="我有点认可你了"
                            }
                            in 25..30->{
                                tip.text="骚年竟恐怖如斯"
                            }
                        }
                    }

                }

            }
        Handler(Looper.getMainLooper()).postDelayed({
            dwView.start()
            openSLRecorder.start("aaa")
            openSLRecorder.addProgressListener(object : OpenSLRecorder.IProgressListener{
                override fun onProgress(recorderMs: Long) {

                }

                override fun onWave(db: Int) {
                   runOnUiThread {
                       if (db<45){
                           dwView.setStart(0.0)
                       }else if(db<60){
                           dwView.setStart((db-45)/100.00)
                       } else if (db>75){
                           dwView.setStart(1.0)
                       }else{
                           dwView.setStart((db-55)/25.00)
                       }
                   }
                }
            })
        },2000)

    }

    override fun onDestroy() {
        dwView.stop()
        openSLRecorder.stop()
        super.onDestroy()
    }
}