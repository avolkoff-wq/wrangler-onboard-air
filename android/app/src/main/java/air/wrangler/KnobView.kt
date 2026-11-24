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
    private val inner get() = rad * 0.60f   // the face is the button

    // drawn like the Wrangler JL climate-control knob (owner): a scale arc on the dash, a black rubber grip with fine ribs,
    // a thin chrome bezel, a matte black face with a pointer that turns; the face is the button
    private val jeepFont = Typeface.create(Typeface.create("sans-serif-condensed", Typeface.BOLD), 800, false)
    override fun onDraw(c: Canvas) {
        val R = rad; val x = cx; val y = cy
        val grip = R * 0.80f; val bezel = R * 0.64f; val face = R * 0.60f
        // dash scale: a 270 degree arc, blue to orange like the temperature scale, with ticks
        r.set(x - R, y - R, x + R, y + R)
        p.style = Paint.Style.STROKE; p.strokeWidth = R * 0.05f; p.strokeCap = Paint.Cap.ROUND
        p.shader = android.graphics.SweepGradient(x, y, intArrayOf(0xFF0A84FF.toInt(), 0xFF64D2FF.toInt(), 0xFFFFD60A.toInt(), 0xFFFF9F0A.toInt(), 0xFFFF453A.toInt(), 0xFF0A84FF.toInt()),
            floatArrayOf(0f, 0.2f, 0.45f, 0.6f, 0.75f, 1f))
        c.save(); c.rotate(135f, x, y)
        r.set(x - R * 0.95f, y - R * 0.95f, x + R * 0.95f, y + R * 0.95f)
        c.drawArc(r, 0f, 270f, false, p)
        c.restore(); p.shader = null
        p.strokeWidth = 1.5f * dp; p.color = 0xFF8E8E93.toInt()
        for (i in 0..27) {
            val a = Math.toRadians((135 + i * 10).toDouble())
            val r0 = R * 0.86f; val r1 = R * (if (i % 3 == 0) 0.89f else 0.875f)
            c.drawLine(x + (r0 * cos(a)).toFloat(), y + (r0 * sin(a)).toFloat(), x + (r1 * cos(a)).toFloat(), y + (r1 * sin(a)).toFloat(), p)
        }
        p.strokeCap = Paint.Cap.BUTT; p.style = Paint.Style.FILL
        // drop shadow of the knob on the dash
        p.color = Color.BLACK
        p.shader = RadialGradient(x, y + R * 0.06f, grip * 1.12f, intArrayOf(0xCC000000.toInt(), 0x00000000), floatArrayOf(0.8f, 1f), Shader.TileMode.CLAMP)
        c.drawCircle(x, y + R * 0.06f, grip * 1.12f, p); p.shader = null
        // rubber grip: dark, lit from above, 72 fine ribs that turn with the knob
        p.shader = LinearGradient(x, y - grip, x, y + grip, 0xFF3C3D42.toInt(), 0xFF0E0E10.toInt(), Shader.TileMode.CLAMP)
        c.drawCircle(x, y, grip, p); p.shader = null
        p.strokeWidth = 1.2f * dp
        for (i in 0 until 72) {
            val a = Math.toRadians((angle + i * 5).toDouble())
            val lit = (0.5f - 0.5f * sin(a).toFloat())   // ribs at the top catch the light
            p.color = (((40 + 70 * lit).toInt()) shl 24) or 0xFFFFFF
            c.drawLine(x + (bezel * 1.04f * cos(a)).toFloat(), y + (bezel * 1.04f * sin(a)).toFloat(), x + (grip * 0.98f * cos(a)).toFloat(), y + (grip * 0.98f * sin(a)).toFloat(), p)
        }
        // chrome bezel
        p.shader = android.graphics.SweepGradient(x, y, intArrayOf(0xFFE8E8ED.toInt(), 0xFF6E6E73.toInt(), 0xFFF2F2F7.toInt(), 0xFF48484A.toInt(), 0xFFE8E8ED.toInt()), null)
        c.drawCircle(x, y, bezel, p); p.shader = null
        // the face: matte black, slightly domed; darker while pressed
        val pressed = mode == 2
        p.shader = RadialGradient(x - face * 0.25f, y - face * 0.35f, face * 1.3f,
            if (pressed) 0xFF1C1C1E.toInt() else 0xFF2E2F33.toInt(), 0xFF09090A.toInt(), Shader.TileMode.CLAMP)
        c.drawCircle(x, y, face * (if (pressed) 0.98f else 1f), p); p.shader = null
        // pointer notch on the face, turns with the knob
        val pa = Math.toRadians((angle - 90).toDouble())
        p.strokeWidth = 3.2f * dp; p.strokeCap = Paint.Cap.ROUND; p.color = if (enabledLink) accent else 0xFF636366.toInt()
        c.drawLine(x + (face * 0.72f * cos(pa)).toFloat(), y + (face * 0.72f * sin(pa)).toFloat(), x + (face * 0.9f * cos(pa)).toFloat(), y + (face * 0.9f * sin(pa)).toFloat(), p)
        p.strokeCap = Paint.Cap.BUTT
        // hold progress on the bezel
        if (pressed && !holdSent) {
            val held = SystemClock.uptimeMillis() - downAt
            if (held > 300) {
                val f = ((held - 300) / 1700f).coerceIn(0f, 1f)
                p.style = Paint.Style.STROKE; p.strokeWidth = 4 * dp; p.strokeCap = Paint.Cap.ROUND; p.color = 0xFFFF9F0A.toInt()
                r.set(x - bezel, y - bezel, x + bezel, y + bezel)
                c.drawArc(r, -90f, 360f * f, false, p)
                p.strokeCap = Paint.Cap.BUTT; p.style = Paint.Style.FILL
            }
            postInvalidateOnAnimation()
        }
        p.style = Paint.Style.FILL
        p.typeface = jeepFont; p.textSize = face * 0.28f; p.color = if (enabledLink) Color.WHITE else 0xFF636366.toInt()
        p.letterSpacing = 0.06f
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
