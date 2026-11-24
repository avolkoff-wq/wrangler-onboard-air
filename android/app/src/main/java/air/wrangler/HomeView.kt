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
import android.view.View
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/**
 * The main screen, drawn natively for the phone (owner: "Apple style"). Everything comes from the DinMeter:
 * STATE (preset, target, live pressure, run state, error, armed) and the preset icon strokes. Nothing is decided here.
 *  idle   : the preset scene, its name, the target in big thin digits, the live line and what a start would do
 *  run    : a progress ring from the start pressure to the target with the live value, phase and time
 *  result : green Done / Already there / orange Off target
 *  error  : a red card with what happened and the live line pressure
 */
class HomeView(ctx: Context) : View(ctx) {
    var state: Ble.State? = null
        set(v) { field = v; invalidate() }
    var iconStrokes: List<Frame.IconItem>? = null
        set(v) { field = v; invalidate() }
    var online = false
        set(v) { field = v; invalidate() }

    private val dp = resources.displayMetrics.density
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val thin = Typeface.create("sans-serif-thin", Typeface.NORMAL)
    private val light = Typeface.create("sans-serif-light", Typeface.NORMAL)
    private val regular = Typeface.create("sans-serif", Typeface.NORMAL)
    private val medium = Typeface.create("sans-serif-medium", Typeface.NORMAL)
    private val r = RectF()
    private var ringShown = 0f   // eased progress

    companion object {
        val PRESETS = listOf("Highway", "Rough road", "Gravel", "Trail", "Rock", "Sand", "Recovery", "Custom")
        const val ORANGE = 0xFFFF9F0A.toInt(); const val BLUE = 0xFF0A84FF.toInt(); const val GREEN = 0xFF30D158.toInt()
        const val RED = 0xFFFF453A.toInt(); const val GREY = 0xFF8E8E93.toInt(); const val GREY2 = 0xFF48484A.toInt()
        const val YELLOW = 0xFFD7FF3A.toInt()
        // PState: IDLE, SETTLE, MEASURE, CAL_PULSE, MAIN, TOPUP, VENT, DONE, ERROR
        fun phase(s: Int) = when (s) { 1 -> "Settling"; 2 -> "Measuring"; 3 -> "Learning"; 4 -> "Pumping"; 5 -> "Top-up"; 6 -> "Venting"; else -> "" }
        // PErr: NONE, SENSOR, OVER, LEAK, TIMEOUT, LOW_P, LOW_V, VALVE, PUMP_ON
        fun error(e: Int) = when (e) {
            1 -> "Sensor fault" to "No valid signal from the pressure sensor"
            2 -> "Over 3.0 bar" to "Stopped: the pressure went over the limit"
            3 -> "No pressure rise" to "Leak, or the hose is not connected?"
            4 -> "Timeout" to "The run took longer than 8 minutes"
            5 -> "Pump up first" to "Learn deflate needs at least 1.5 bar"
            6 -> "Low voltage" to "AUX1 power sagged below 11.5 V"
            7 -> "Valve stuck open" to "Pressure falls while the valve is shut"
            8 -> "Pressure rising" to "No signal from us. AUX2 on? Else a relay is stuck"
            else -> "Error" to ""
        }
        fun mmss(ms: Long): String { val s = ms / 1000; return "%d:%02d".format(s / 60, s % 60) }
    }

    override fun onDraw(c: Canvas) {
        val s = state
        val w = width.toFloat(); val h = height.toFloat()
        c.drawColor(Color.BLACK)
        if (s == null) { centered(c, "Looking for WranglerAir", h / 2, 17f, GREY, regular); return }
        val running = s.state in 1..6
        val accent = when {
            s.state == 8 -> RED
            s.state == 7 -> if (s.offTarget) ORANGE else GREEN
            s.valve -> BLUE
            running -> ORANGE
            else -> 0xFF2C2C2E.toInt()
        }
        // soft glow of the state colour behind everything
        p.shader = RadialGradient(w / 2, h * 0.35f, max(w, h) * 0.75f, (accent and 0x00FFFFFF) or 0x55000000, Color.TRANSPARENT, Shader.TileMode.CLAMP)
        c.drawRect(0f, 0f, w, h, p); p.shader = null

        when {
            s.state == 8 -> drawError(c, s, w, h)
            running -> drawRun(c, s, w, h, accent)
            else -> drawIdle(c, s, w, h)
        }
        if (!online) { p.color = 0x99000000.toInt(); c.drawRect(0f, 0f, w, h, p) }
        if (running || abs(ringShown - target(s)) > 0.002f) postInvalidateOnAnimation()
    }

    private fun target(s: Ble.State): Float {
        val span = s.target - s.p0
        val cur = if (s.state == 4 || s.state == 5) s.est else s.bar
        return if (abs(span) < 0.01f) 1f else ((cur - s.p0) / span).coerceIn(0f, 1f)
    }

    // ---------- idle and results ----------
    private fun drawIdle(c: Canvas, s: Ble.State, w: Float, h: Float) {
        // scene: the DinMeter's own preset picture, big and crisp
        val iconW = min(w * 0.84f, h * 0.34f / 0.62f)
        val iconH = iconW * 0.62f
        val ix = (w - iconW) / 2; val iy = h * 0.09f
        iconStrokes?.let { strokes ->
            c.save(); c.translate(ix, iy)
            val box = strokesBox(strokes)
            val k = iconW / box
            c.scale(k, k)
            IconPainter.draw(c, 0f, 0f, strokes)
            c.restore()
        }
        var y = iy + iconH + h * 0.08f
        val name = PRESETS.getOrElse(s.preset) { "Preset" }.uppercase()
        p.letterSpacing = 0.18f
        centered(c, name, y, 15f, if (s.target < 0.8f) ORANGE else GREY, medium)
        p.letterSpacing = 0f
        y += h * 0.16f
        // target in big thin digits + unit
        p.typeface = thin; p.textSize = min(h * 0.17f, w * 0.34f); p.color = Color.WHITE
        val num = "%.1f".format(s.target)
        val nw = p.measureText(num)
        val unitSize = p.textSize * 0.22f
        val pu = Paint(p).apply { typeface = light; textSize = unitSize; color = GREY }
        val total = nw + 8 * dp + pu.measureText("bar")
        val x0 = (w - total) / 2
        c.drawText(num, x0, y, p)
        c.drawText("bar", x0 + nw + 8 * dp, y, pu)
        y += h * 0.085f
        // the line below: what a start would do, the result, or armed
        val (line, col) = when {
            s.state == 7 && s.inBand -> "Already within 0.1 bar" to GREEN
            s.state == 7 && s.offTarget -> "Off target: %.2f bar".format(s.staticBar) to ORANGE
            s.state == 7 -> "Done: %.2f bar in %s".format(s.staticBar, mmss(s.runMs)) to GREEN
            s.armStage == 2 -> "Engine off? Press again to start" to RED
            s.armStage == 1 -> "Press again to start" to Color.WHITE
            s.noAir -> "Connect the system" to GREY
            abs(s.bar - s.target) <= 0.1f -> "Now %.1f bar  ·  OK".format(s.bar) to GREEN
            s.bar < s.target -> "Now %.1f bar  ·  ▲ Inflate".format(s.bar) to ORANGE
            else -> "Now %.1f bar  ·  ▼ Deflate".format(s.bar) to BLUE
        }
        if (s.armStage > 0 || s.state == 7) pill(c, line, y, col) else centered(c, line, y, 19f, col, regular)
        if (s.demo || s.fake) centered(c, if (s.demo) "DEMO" else "FAKE SENSOR", h - 10 * dp, 12f, 0xFFFF2D9F.toInt(), medium)
    }

    // the icon strokes: the box is 52 units wide on the DinMeter; take the largest x reached as the box width
    private fun strokesBox(st: List<Frame.IconItem>): Float {
        var mx = 1f
        for (i in st) mx = max(mx, max(i.a, i.c) / 8f)
        return mx
    }

    // ---------- run ----------
    private fun drawRun(c: Canvas, s: Ble.State, w: Float, h: Float, accent: Int) {
        val goal = target(s)
        ringShown += (goal - ringShown) * 0.15f
        val d = min(w * 0.78f, h * 0.62f)
        val cx = w / 2; val cy = h * 0.42f
        val stroke = d * 0.075f
        r.set(cx - d / 2, cy - d / 2, cx + d / 2, cy + d / 2)
        p.style = Paint.Style.STROKE; p.strokeWidth = stroke; p.strokeCap = Paint.Cap.ROUND
        p.color = 0xFF1C1C1E.toInt(); c.drawArc(r, 0f, 360f, false, p)
        p.shader = LinearGradient(r.left, r.top, r.right, r.bottom, accent, (accent and 0x00FFFFFF) or 0xFF000000.toInt(), Shader.TileMode.CLAMP)
        p.color = accent
        c.drawArc(r, -90f, 360f * ringShown.coerceAtLeast(0.005f), false, p)
        p.shader = null; p.style = Paint.Style.FILL; p.strokeCap = Paint.Cap.BUTT
        // live value: the tyre estimate while pumping, else the line
        val est = s.state == 4 || s.state == 5
        val v = if (est) s.est else s.bar
        p.typeface = thin; p.textSize = d * 0.30f; p.color = Color.WHITE
        val txt = (if (est) "~" else "") + "%.2f".format(v)
        c.drawText(txt, cx - p.measureText(txt) / 2, cy + p.textSize * 0.32f, p)
        centered(c, "bar  ·  target %.1f".format(s.target), cy + d * 0.22f, 15f, GREY, regular)
        val ph = phase(s.state).uppercase()
        p.letterSpacing = 0.16f
        centered(c, ph, cy - d * 0.2f, 14f, accent, medium)
        p.letterSpacing = 0f
        centered(c, mmss(s.runMs), cy + d / 2 + h * 0.08f, 22f, Color.WHITE, light)
        centered(c, "Tap the knob to stop", cy + d / 2 + h * 0.13f, 14f, GREY, regular)
    }

    // ---------- error ----------
    private fun drawError(c: Canvas, s: Ble.State, w: Float, h: Float) {
        val (title, sub) = error(s.err)
        val m = 20 * dp
        r.set(m, h * 0.12f, w - m, h * 0.62f)
        p.color = 0xFF2A0F0E.toInt(); c.drawRoundRect(r, 28 * dp, 28 * dp, p)
        p.style = Paint.Style.STROKE; p.strokeWidth = 1.5f * dp; p.color = (RED and 0x00FFFFFF) or 0x88000000.toInt()
        c.drawRoundRect(r, 28 * dp, 28 * dp, p); p.style = Paint.Style.FILL
        // warning glyph
        val gy = r.top + r.height() * 0.2f
        p.color = RED; c.drawCircle(w / 2, gy, 24 * dp, p)
        centered(c, "!", gy + 10 * dp, 30f, Color.WHITE, medium)
        centered(c, title, r.top + r.height() * 0.46f, 28f, Color.WHITE, medium)
        wrapped(c, sub, r.top + r.height() * 0.6f, 16f, 0xFFFFB4AE.toInt(), r.width() - 32 * dp)
        p.typeface = thin; p.textSize = h * 0.12f; p.color = Color.WHITE
        val v = "%.2f".format(s.bar)
        c.drawText(v, w / 2 - p.measureText(v) / 2, h * 0.8f, p)
        centered(c, "bar in the line now", h * 0.86f, 15f, GREY, regular)
    }

    // ---------- text helpers ----------
    private fun centered(c: Canvas, t: String, y: Float, sp: Float, col: Int, tf: Typeface) {
        p.typeface = tf; p.textSize = sp * dp; p.color = col
        var tw = p.measureText(t)
        if (tw > width - 24 * dp) { p.textSize *= (width - 24 * dp) / tw; tw = p.measureText(t) }
        c.drawText(t, (width - tw) / 2, y, p)
    }
    private fun wrapped(c: Canvas, t: String, y0: Float, sp: Float, col: Int, maxW: Float) {
        p.typeface = regular; p.textSize = sp * dp; p.color = col
        val words = t.split(" "); var line = ""; var y = y0
        for (wd in words) {
            val cand = if (line.isEmpty()) wd else "$line $wd"
            if (p.measureText(cand) > maxW && line.isNotEmpty()) { c.drawText(line, (width - p.measureText(line)) / 2, y, p); y += sp * dp * 1.35f; line = wd }
            else line = cand
        }
        if (line.isNotEmpty()) c.drawText(line, (width - p.measureText(line)) / 2, y, p)
    }
    private fun pill(c: Canvas, t: String, y: Float, col: Int) {
        p.typeface = medium; p.textSize = 17 * dp
        val tw = p.measureText(t); val ph = 40 * dp
        r.set((width - tw) / 2 - 20 * dp, y - ph * 0.7f, (width + tw) / 2 + 20 * dp, y + ph * 0.3f)
        p.color = (col and 0x00FFFFFF) or 0x33000000; c.drawRoundRect(r, ph / 2, ph / 2, p)
        p.color = col; c.drawText(t, (width - tw) / 2, y, p)
    }
}
