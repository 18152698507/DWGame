package cn.zybwz.dwgame

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.View
import com.jiayz.dwgame.R
import java.util.*


class DWView:View {
    private lateinit var zzbitmap:Bitmap
    private lateinit var pzbitmap:Bitmap
    private val paint=Paint()
    private val point:Point= Point(0,30)
    private val pzpoint:Point= Point(0,0)
    private fun initSrc(){
        paint.textSize=60f
        val matrix = Matrix()
        val srczz=BitmapFactory.decodeResource(resources, R.drawable.zz)
        matrix.postScale(90f/srczz.width,90f/srczz.height)
        zzbitmap = Bitmap.createBitmap(
            srczz, 0, 0,
            srczz.width, srczz.height, matrix, true
        )
        srczz.recycle()
        val rd = Random()
        val nextInt = rd.nextInt(width-90)
        point.x=nextInt

        val matrixPZ = Matrix()
        val srcpz=BitmapFactory.decodeResource(resources, R.drawable.lz
        )
        matrixPZ.postScale(120f/srczz.width,120f/srczz.height)
        pzbitmap = Bitmap.createBitmap(
            srcpz, 0, 0,
            srcpz.width, srcpz.height, matrixPZ, true
        )
        srcpz.recycle()

        pzpoint.set(0,height-150)
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        initSrc()
    }

    constructor(context: Context):super(context){}

    constructor(context: Context,attributeSet: AttributeSet):super(context,attributeSet){}

    override fun onDraw(canvas: Canvas?) {
        super.onDraw(canvas)
        canvas?.drawBitmap(zzbitmap,point.x.toFloat(),point.y.toFloat(),paint)
        canvas?.drawBitmap(pzbitmap,pzpoint.x.toFloat(),pzpoint.y.toFloat(),paint)
        canvas?.drawText("吃了 $count 个粽子,还有 $all 个赶紧吃",10f,50f,paint)
    }

    private var playing=false
    private var count=0;
    private var all=30;
    var result:Result?=null

    fun start(){
        playing=true
        count=0;
        synchronized(point){
            Thread{
                while (playing){
                    point.y+=10+count
                    if (point.y>height-120&&point.x>pzpoint.x&&point.x<pzpoint.x+240f){
                        count++
                        point.y=30
                        val rd = Random()
                        val nextInt = rd.nextInt(width-60)
                        point.x=nextInt
                        all--
                    }
                    if (point.y>height){
                        point.y=30
                        val rd = Random()
                        val nextInt = rd.nextInt(width-90)
                        point.x=nextInt
                        all--
                    }
                    if (all<=0){
                        playing=false
                        result?.onFinish(count)
                    }

                    Thread.sleep(16)
                    postInvalidate()
                }
            }.start()
        }
    }

    fun stop(){
        playing=false
    }

    fun setStart(start:Double){
        pzpoint.x=(start*(width-240)).toInt()
        postInvalidate()
    }

    interface Result{
        fun onFinish(count:Int)
    }
}