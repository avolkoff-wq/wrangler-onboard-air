package air.wrangler

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import android.os.SystemClock
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sin

/**
 * The DinMeter knob on the phone. Drag the ring around: one step per 18 degrees (clockwise = right), a haptic tick each.
 * The centre is the knob button: tap = press, 0.5-2 s = medium press, held 2 s = hold (menu / back), with a ring filling.
 */
class KnobView(ctx: Context, private val onStep: (Int) -> Unit, private val onButton: (Int) -> Unit) : View(ctx) {
    var label = "PRESS"
        set(v) { if (field != v) { field = v; invalidate() } }
    var accent = 0xFF30D158.toInt()
        set(v) { if (field != v) { field = v; invalidate() } }
    var enabledLink = false
        set(v) { if (field != v) { field = v; invalidate() } }

    private val dp = resources.displayMetrics.density
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val r = RectF()
    private val medium = Typeface.create("sans-serif-medium", Typeface.NORMAL)
    private var angle = 0f            // drawn rotation of the ring
    private var lastA = 0f; private var acc = 0f
    private var mode = 0              // 0 none, 1 turning, 2 button
    private var downAt = 0L; private var holdSent = false
    private val STEP = 18f

    private val cx get() = width / 2f
    private val cy get() = height / 2f
    private val rad get() = min(width, height) / 2f - 10 * dp
    private val inner get() = rad * 0.52f

    override fun onDraw(c: Canvas) {
        val R = rad; val x = cx; val y = cy
        // outer ring: brushed metal look, notches every 18 degrees
        p.shader = RadialGradient(x, y - R * 0.3f, R * 1.3f, 0xFF3A3A3C.toInt(), 0xFF101012.toInt(), Shader.TileMode.CLAMP)
        c.drawCircle(x, y, R, p); p.shader = null
        p.style = Paint.Style.STROKE; p.strokeWidth = 1.2f * dp; p.color = 0xFF5A5A5E.toInt()
        c.drawCircle(x, y, R - 0.6f * dp, p)
        p.strokeWidth = 2.2f * dp; p.strokeCap = Paint.Cap.ROUND
        for (i in 0 until 20) {
            val a = Math.toRadians((angle + i * STEP).toDouble())
            p.color = if (i == 0) accent else 0xFF6E6E73.toInt()
            val r0 = R * (if (i == 0) 0.76f else 0.82f); val r1 = R * 0.92f
            c.drawLine(x + (r0 * sin(a)).toFloat(), y - (r0 * cos(a)).toFloat(), x + (r1 * sin(a)).toFloat(), y - (r1 * cos(a)).toFloat(), p)
        }
        p.style = Paint.Style.FILL; p.strokeCap = Paint.Cap.BUTT
        // button
        val I = inner
        val pressed = mode == 2
        p.shader = LinearGradient(x, y - I, x, y + I, if (pressed) 0xFF1C1C1E.toInt() else 0xFF2C2C2E.toInt(), 0xFF141416.toInt(), Shader.TileMode.CLAMP)
        c.drawCircle(x, y, I, p); p.shader = null
        p.style = Paint.Style.STROKE; p.strokeWidth = 2 * dp; p.color = if (enabledLink) accent else 0xFF3A3A3C.toInt()
        c.drawCircle(x, y, I - dp, p)
        // hold progress
        if (pressed && !holdSent) {
            val held = SystemClock.uptimeMillis() - downAt
            if (held > 300) {
                val f = ((held - 300) / 1700f).coerceIn(0f, 1f)
                p.strokeWidth = 5 * dp; p.strokeCap = Paint.Cap.ROUND; p.color = Color.WHITE
                r.set(x - I - 9 * dp, y - I - 9 * dp, x + I + 9 * dp, y + I + 9 * dp)
                c.drawArc(r, -90f, 360f * f, false, p)
                p.strokeCap = Paint.Cap.BUTT
            }
            postInvalidateOnAnimation()
        }
        p.style = Paint.Style.FILL
        p.typeface = medium; p.textSize = I * 0.30f; p.color = if (enabledLink) Color.WHITE else 0xFF636366.toInt()
        p.letterSpacing = 0.08f
        val t = if (pressed && SystemClock.uptimeMillis() - downAt > 300 && !holdSent) "MENU" else label
        c.drawText(t, x - p.measureText(t) / 2, y + p.textSize * 0.36f, p)
        p.letterSpacing = 0f
    }

    private fun ang(e: MotionEvent) = Math.toDegrees(atan2((e.x - cx).toDouble(), -(e.y - cy).toDouble())).toFloat()

    override fun onTouchEvent(e: MotionEvent): Boolean {
        val d = hypot(e.x - cx, e.y - cy)
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (d <= inner) { mode = 2; downAt = SystemClock.uptimeMillis(); holdSent = false; postDelayed(holdTick, 50); invalidate() }
                else if (d <= rad * 1.15f) { mode = 1; lastA = ang(e); acc = 0f }
                else return false
                parent.requestDisallowInterceptTouchEvent(true)
                return true
            }
            MotionEvent.ACTION_MOVE -> if (mode == 1) {
                val a = ang(e); var da = a - lastA
                if (da > 180) da -= 360; if (da < -180) da += 360
                lastA = a; acc += da; angle += da
                while (acc >= STEP) { acc -= STEP; onStep(1); performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK) }
                while (acc <= -STEP) { acc += STEP; onStep(-1); performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK) }
                invalidate()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (mode == 2 && e.actionMasked == MotionEvent.ACTION_UP && !holdSent) {
                    val held = SystemClock.uptimeMillis() - downAt
                    onButton(if (held < 500) Ble.PRESS else Ble.MID)
                    performHapticFeedback(HapticFeedbackConstants.CONFIRM)
                }
                mode = 0; removeCallbacks(holdTick); invalidate()
            }
        }
        return true
    }

    private val holdTick = object : Runnable {
        override fun run() {
            if (mode != 2 || holdSent) return
            if (SystemClock.uptimeMillis() - downAt >= 2000) {
                holdSent = true; onButton(Ble.HOLD); performHapticFeedback(HapticFeedbackConstants.LONG_PRESS); invalidate(); return
            }
            postDelayed(this, 50)
        }
    }
}
