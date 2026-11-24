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
class HomeView(ctx: Context, private val onTurn: (Int) -> Unit = {}) : View(ctx) {
    // owner: swipe across the picture to change the preset, like flipping cards: one preset per 70 dp, finger to the
    // left = the next preset in the list (lower pressure), to the right = the previous one; a haptic tick each
    private var downX = 0f; private var stepped = 0; private var swiping = false
    override fun onTouchEvent(e: android.view.MotionEvent): Boolean {
        val s = state ?: return false
        if (s.state in 1..8 && s.state != 7) return false          // not during a run or on an error
        val step = 70 * dp
        when (e.actionMasked) {
            android.view.MotionEvent.ACTION_DOWN -> { downX = e.x; stepped = 0; swiping = false; return true }
            android.view.MotionEvent.ACTION_MOVE -> {
                val want = ((downX - e.x) / step).toInt()
                if (want != 0) { swiping = true; parent.requestDisallowInterceptTouchEvent(true) }
                while (stepped < want) { onTurn(-1); stepped++; performHapticFeedback(android.view.HapticFeedbackConstants.CLOCK_TICK) }
                while (stepped > want) { onTurn(1); stepped--; performHapticFeedback(android.view.HapticFeedbackConstants.CLOCK_TICK) }
                return true
            }
        }
        return true
    }

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
        else if (isShown) postInvalidateDelayed(40)   // the scene moves slowly (clouds, dashes, dust): 25 frames a second
    }

    private fun target(s: Ble.State): Float {
        val span = s.target - s.p0
        val cur = if (s.state == 4 || s.state == 5) s.est else s.bar
        return if (abs(span) < 0.01f) 1f else ((cur - s.p0) / span).coerceIn(0f, 1f)
    }

    // ---------- idle and results ----------
    // Jeep style (owner): heavy condensed capitals for the name, the numbers and the line under them
    private val jeepFont: Typeface = Typeface.create(Typeface.create("sans-serif-condensed", Typeface.BOLD), 800, false)
    private fun drawIdle(c: Canvas, s: Ble.State, w: Float, h: Float) {
        val y0 = drawCard(c, s, w, h, null)
        idleBelow(c, s, w, h, y0)
    }
    /** the scene card on top of every main-screen mode (idle, run, result, error): one style everywhere (owner).
     *  tint: a colour laid over the scene (red on an error); returns the y under the card */
    private fun drawCard(c: Canvas, s: Ble.State, w: Float, h: Float, tint: Int?, running: Boolean = false, progress: Float = -1f, accent: Int = 0): Float {
        // the scene card (advisor: taller, hairline edge, the link state as a small capsule inside it)
        val m = 16 * dp
        val cw = w - 2 * m
        val ch = min(cw * 0.62f, h * 0.44f)
        val top = 4 * dp
        c.save()
        r.set(m, top, m + cw, top + ch)
        val clip = android.graphics.Path().apply { addRoundRect(r, 24 * dp, 24 * dp, android.graphics.Path.Direction.CW) }
        c.clipPath(clip); c.translate(m, top)
        SceneArt.draw(c, s.preset, cw, ch, time = (android.os.SystemClock.uptimeMillis() % 3_600_000L) / 1000f, running = running || tint != null, progress = progress, accent = accent)
        c.restore()
        p.style = Paint.Style.STROKE; p.strokeWidth = 1 * dp; p.color = 0x1AFFFFFF
        r.set(m, top, m + cw, top + ch); c.drawRoundRect(r, 24 * dp, 24 * dp, p); p.style = Paint.Style.FILL
        if (online) {   // link capsule, top left in the card
            p.typeface = medium; p.textSize = 12 * dp
            val lt = "Connected"; val lw = p.measureText(lt)
            r.set(m + 12 * dp, top + 12 * dp, m + 12 * dp + lw + 30 * dp, top + 36 * dp)
            p.color = 0x66000000; c.drawRoundRect(r, 12 * dp, 12 * dp, p)
            p.color = GREEN; c.drawCircle(r.left + 12 * dp, r.centerY(), 4 * dp, p)
            p.color = 0xFFEBEBF5.toInt(); c.drawText(lt, r.left + 22 * dp, r.centerY() + 4.3f * dp, p)
        }
        if (tint != null) { r.set(m, top, m + cw, top + ch); c.save(); c.clipPath(clip); p.color = tint; c.drawRect(r, p); c.restore() }
        return top + ch
    }
    private fun idleBelow(c: Canvas, s: Ble.State, w: Float, h: Float, cardBottom: Float) {
        // page dots: eight presets, the swipe affordance
        var y = cardBottom + 16 * dp
        val n = PRESETS.size; val gap = 14 * dp
        val dx0 = w / 2 - gap * (n - 1) / 2
        for (k in 0 until n) { p.color = if (k == s.preset) Color.WHITE else 0xFF48484A.toInt(); c.drawCircle(dx0 + k * gap, y, 3 * dp, p) }
        if (s.screen == 7) { drawCustomEdit(c, s, w, y); return }
        // name: owner wants it big and heavy, Jeep style
        y += 44 * dp + centre(y, 30f, 38f)      // owner: the text block sits midway between the picture and the knob
        val name = PRESETS.getOrElse(s.preset) { "Preset" }.uppercase()
        p.letterSpacing = 0.08f
        heavy { centeredF(c, name, y, 34f, if (s.target < 0.8f) ORANGE else 0xFFF5F5F7.toInt()) }
        p.letterSpacing = 0f
        // target: big white heavy digits + BAR (advisor: one accent per state, no neon)
        val numSize = min(h * 0.18f, w * 0.40f)
        y += numSize * 0.92f
        p.typeface = jeepFont; p.textSize = numSize; p.color = 0xFFF5F5F7.toInt()
        val num = "%.1f".format(s.target)
        p.textScaleX = 0.9f
        val nw = p.measureText(num)
        val pu = Paint(p).apply { textSize = numSize * 0.22f; color = GREY; letterSpacing = 0.08f; textScaleX = 1f; style = Paint.Style.FILL }
        val total = nw + 8 * dp + pu.measureText("BAR")
        val x0 = (w - total) / 2
        heavy { c.drawText(num, x0, y, p) }
        p.textScaleX = 1f
        c.drawText("BAR", x0 + nw + 8 * dp, y, pu)
        y += 34 * dp
        val (line, col) = when {
            s.state == 7 && s.inBand -> "ALREADY WITHIN 0.1 BAR" to GREEN
            s.state == 7 && s.offTarget -> "OFF TARGET: %.2f BAR".format(s.staticBar) to ORANGE
            s.state == 7 -> "DONE: %.2f BAR IN %s".format(s.staticBar, mmss(s.runMs)) to GREEN
            s.armStage == 2 -> "ENGINE OFF? PRESS AGAIN" to RED
            s.armStage == 1 -> "PRESS AGAIN TO START" to Color.WHITE
            s.noAir -> "NO PRESSURE  ·  CONNECT THE HOSE" to GREY
            abs(s.bar - s.target) <= 0.1f -> "NOW %.1f BAR  ·  OK".format(s.bar) to GREEN
            s.bar < s.target -> "NOW %.1f BAR  ·  ▲ INFLATE".format(s.bar) to ORANGE
            else -> "NOW %.1f BAR  ·  ▼ DEFLATE".format(s.bar) to BLUE
        }
        p.letterSpacing = 0.1f
        if (s.armStage > 0 || s.state == 7) pill(c, line, y, col) else centeredF(c, line, y, 15f, col)
        p.letterSpacing = 0f
        if (s.demo || s.fake) centeredF(c, if (s.demo) "DEMO" else "FAKE SENSOR", h - 10 * dp, 13f, 0xFFFF2D9F.toInt())
    }
    /** the CUSTOM editor (DinMeter screen CUSTOM): step 0 the pressure in 0.1 bar, step 1 the wheels on the line */
    private fun drawCustomEdit(c: Canvas, s: Ble.State, w: Float, y0: Float) {
        var y = y0 + 44 * dp + centre(y0, 24f, 62f)
        p.letterSpacing = 0.08f
        heavy { centeredF(c, if (s.customStep == 0) "CUSTOM PRESSURE" else "WHEELS ON THE LINE", y, 26f, 0xFF64D2FF.toInt()) }
        p.letterSpacing = 0f
        val numSize = min(height * 0.18f, w * 0.40f)
        y += numSize * 0.92f
        p.typeface = jeepFont; p.textSize = numSize; p.color = 0xFF64D2FF.toInt(); p.textScaleX = 0.9f
        val num = if (s.customStep == 0) "%.1f".format(s.customEdit) else "${s.customWheels}"
        val unit = if (s.customStep == 0) "BAR" else if (s.customWheels == 1) "WHEEL" else "WHEELS"
        val nw = p.measureText(num)
        val pu = Paint(p).apply { textSize = numSize * 0.22f; color = GREY; letterSpacing = 0.08f; textScaleX = 1f; style = Paint.Style.FILL }
        val x0 = (w - nw - 8 * dp - pu.measureText(unit)) / 2
        heavy { c.drawText(num, x0, y, p) }
        p.textScaleX = 1f
        c.drawText(unit, x0 + nw + 8 * dp, y, pu)
        // step dots: value, wheels
        y += 28 * dp
        for (k in 0..1) { p.color = if (k == s.customStep) 0xFF64D2FF.toInt() else 0xFF48484A.toInt(); r.set(w / 2 - 34 * dp + k * 38 * dp, y - 3 * dp, w / 2 - 4 * dp + k * 38 * dp, y + 3 * dp); c.drawRoundRect(r, 3 * dp, 3 * dp, p) }
        y += 30 * dp
        p.letterSpacing = 0.1f
        centeredF(c, if (s.customStep == 0) "TURN: 0.1 BAR  ·  PRESS: NEXT  ·  HOLD: CANCEL" else "TURN: WHEELS  ·  PRESS: OK  ·  HOLD: BACK", y, 13f, GREY)
        p.letterSpacing = 0f
    }
    /** how far to move the text block down so it sits midway between the dots line and the bottom of this view (the knob
     *  starts there): the block = the title's cap height, the big number and what follows it (dp) */
    private fun centre(dotsY: Float, titleSp: Float, tailDp: Float): Float {
        val numSize = min(height * 0.18f, width * 0.40f)
        val block = titleSp * 0.72f * dp + numSize * 0.92f + tailDp * dp
        val natural = 44 * dp - titleSp * 0.72f * dp          // the gap the layout leaves above the title without centring
        return (((height - dotsY) - block) / 2 - natural).coerceAtLeast(0f)
    }
    /** a heavier weight than Roboto Condensed Bold has: fill and a thin stroke of the same colour (advisor) */
    private inline fun heavy(f: () -> Unit) {
        p.style = Paint.Style.FILL_AND_STROKE; p.strokeWidth = p.textSize * 0.03f; p.strokeJoin = Paint.Join.MITER
        f()
        p.style = Paint.Style.FILL; p.strokeWidth = 0f
    }
    private fun centeredF(c: Canvas, t: String, y: Float, sp: Float, col: Int) {
        p.typeface = jeepFont; p.textSize = sp * dp; p.color = col
        var tw = p.measureText(t)
        if (tw > width - 24 * dp) { p.textSize *= (width - 24 * dp) / tw; tw = p.measureText(t) }
        if (p.style == Paint.Style.FILL_AND_STROKE) p.strokeWidth = p.textSize * 0.03f
        c.drawText(t, (width - tw) / 2, y, p)
    }

    // the icon strokes: the box is 52 units wide on the DinMeter; take the largest x reached as the box width
    private fun strokesBox(st: List<Frame.IconItem>): Float {
        var mx = 1f
        for (i in st) mx = max(mx, max(i.a, i.c) / 8f)
        return mx
    }

    // ---------- run ----------
    private fun drawRun(c: Canvas, s: Ble.State, w: Float, h: Float, accent: Int) {
        // the same card as idle (the Jeep drives), then the phase, the live value and a progress capsule (owner: one style)
        val goal = target(s)
        ringShown += (goal - ringShown) * 0.15f
        var y = drawCard(c, s, w, h, null, running = true, progress = ringShown, accent = accent) + 16 * dp
        y += 44 * dp + centre(y, 22f, 88f)      // the same line as the preset name on the idle screen
        p.letterSpacing = 0.14f
        heavy { centeredF(c, phase(s.state).uppercase() + "  ·  " + mmss(s.runMs), y, 22f, accent) }
        p.letterSpacing = 0f
        val est = s.state == 4 || s.state == 5
        val v = if (est) s.est else s.bar
        val numSize = min(h * 0.18f, w * 0.40f)
        y += numSize * 0.92f
        p.typeface = jeepFont; p.textSize = numSize; p.color = 0xFFF5F5F7.toInt(); p.textScaleX = 0.9f
        val num = "%.2f".format(v)
        val nw = p.measureText(num)
        val pu = Paint(p).apply { textSize = numSize * 0.22f; color = GREY; letterSpacing = 0.08f; textScaleX = 1f; style = Paint.Style.FILL }
        val x0 = (w - nw - 8 * dp - pu.measureText("BAR")) / 2
        heavy { c.drawText(num, x0, y, p) }
        p.textScaleX = 1f
        c.drawText("BAR", x0 + nw + 8 * dp, y, pu)
        if (est) c.drawText("EST", x0 + nw + 8 * dp, y - numSize * 0.3f, pu)      // the tyre estimate while pumping
        // progress capsule from the start pressure to the target, the two numbers at its ends
        y += 26 * dp
        val bx0 = 32 * dp; val bx1 = w - 32 * dp; val bh = 6 * dp
        r.set(bx0, y, bx1, y + bh); p.color = 0xFF2C2C2E.toInt(); c.drawRoundRect(r, bh / 2, bh / 2, p)
        p.color = 0xFFF5F5F7.toInt(); c.drawRect(bx1 - 2 * dp, y - 3 * dp, bx1, y + bh + 3 * dp, p)   // the target tick
        r.set(bx0, y, bx0 + (bx1 - bx0) * ringShown.coerceIn(0.02f, 1f), y + bh); p.color = accent; c.drawRoundRect(r, bh / 2, bh / 2, p)
        y += bh + 22 * dp
        p.typeface = jeepFont; p.textSize = 14 * dp; p.color = GREY; p.letterSpacing = 0.08f
        c.drawText("%.1f".format(s.p0), bx0, y, p)
        val tt = "TARGET %.1f".format(s.target); c.drawText(tt, bx1 - p.measureText(tt), y, p)
        p.letterSpacing = 0f
        y += 30 * dp
        p.letterSpacing = 0.1f; centeredF(c, "PRESS THE KNOB TO STOP", y, 13f, GREY); p.letterSpacing = 0f
    }

    // ---------- error ----------
    private fun drawError(c: Canvas, s: Ble.State, w: Float, h: Float) {
        // the same card with a light red wash (the Jeep stays grey), the title on the name line, the live line pressure on
        // the same baseline and size as the idle number, then one line of explanation and the hint
        val (title, sub) = error(s.err)
        var y = drawCard(c, s, w, h, 0x4DFF2A1F) + 16 * dp
        y += 44 * dp + centre(y, 30f, 58f)
        p.letterSpacing = 0.08f
        heavy { centeredF(c, title.uppercase(), y, 30f, RED) }
        p.letterSpacing = 0f
        val numSize = min(h * 0.18f, w * 0.40f)
        y += numSize * 0.92f
        p.typeface = jeepFont; p.textSize = numSize; p.color = 0xFFF5F5F7.toInt(); p.textScaleX = 0.9f
        val num = "%.2f".format(s.bar); val nw = p.measureText(num)
        val pu = Paint(p).apply { textSize = numSize * 0.22f; color = GREY; letterSpacing = 0.08f; textScaleX = 1f; style = Paint.Style.FILL }
        val x0 = (w - nw - 8 * dp - pu.measureText("BAR")) / 2
        heavy { c.drawText(num, x0, y, p) }
        p.textScaleX = 1f; c.drawText("BAR", x0 + nw + 8 * dp, y, pu)
        c.drawText("NOW", x0 + nw + 8 * dp, y - numSize * 0.3f, pu)
        y += 30 * dp
        p.letterSpacing = 0.06f; centeredF(c, sub.uppercase(), y, 13f, 0xFFFFB4AE.toInt())
        y += 24 * dp
        p.letterSpacing = 0.1f; centeredF(c, "PRESS: OK", y, 13f, GREY); p.letterSpacing = 0f
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
        p.typeface = jeepFont; p.textSize = 18 * dp
        val tw = p.measureText(t); val ph = 40 * dp
        r.set((width - tw) / 2 - 20 * dp, y - ph * 0.7f, (width + tw) / 2 + 20 * dp, y + ph * 0.3f)
        p.color = (col and 0x00FFFFFF) or 0x33000000; c.drawRoundRect(r, ph / 2, ph / 2, p)
        p.color = col; c.drawText(t, (width - tw) / 2, y, p)
    }
}
