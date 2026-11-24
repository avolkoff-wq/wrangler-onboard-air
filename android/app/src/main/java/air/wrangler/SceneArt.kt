package air.wrangler

import android.graphics.BlurMaskFilter
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.floor
import kotlin.math.sin

/**
 * The preset scenes for the phone (owner: "make it beautiful", "the Jeep stands, the ground runs under it").
 * Same subjects as the DinMeter icons. The Jeep keeps its place; the world scrolls past in layers (far slow, near fast),
 * the Jeep sits on the ground under both wheels, so it pitches and bounces over the relief, and the wheels turn.
 * RECOVERY: stuck, the wheels spin in place and throw sand. Everything in fractions of the w x h box.
 */
object SceneArt {
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path = Path()
    private val r = RectF()
    private var t = 0f
    private val jeep: Path by lazy {
        Path().apply {
            fillType = Path.FillType.EVEN_ODD
            for (c in JeepShape.CONTOURS) {
                moveTo(c[0], c[1]); var i = 2
                while (i < c.size) { lineTo(c[i], c[i + 1]); i += 2 }
                close()
            }
        }
    }
    // JL cues on the silhouette (owner: "it looks like a UAZ Patriot"): black high fender flares, rock rails,
    // steel bumpers and the spare wheel cover; the body and the hardtop in the owner's Sting Gray (his photo, 24.11). Mask coordinates,
    // each cut to the body so it follows the outline.
    private val trim: Path by lazy {
        val r0 = JeepShape.WHEEL_R; val t = Path()
        val flareBand = Path()
        for (cx in floatArrayOf(JeepShape.REAR_X, JeepShape.FRONT_X)) {   // flares: a band of even width round the arch, flat top
            val o = Path(); val top = TYRE_Y - TYRE_R * (ARCH_K + 0.24f)
            o.moveTo(cx - TYRE_R * 1.6f, 150f); o.lineTo(cx - TYRE_R * 0.95f, top); o.lineTo(cx + TYRE_R * 0.95f, top); o.lineTo(cx + TYRE_R * 1.6f, 150f); o.close()
            val inner = Path(); val ti = TYRE_Y - TYRE_R * ARCH_K
            inner.moveTo(cx - TYRE_R * 1.3f, 152f); inner.lineTo(cx - TYRE_R * 0.75f, ti); inner.lineTo(cx + TYRE_R * 0.75f, ti); inner.lineTo(cx + TYRE_R * 1.3f, 152f); inner.close()
            inner.op(arches, Path.Op.UNION)
            o.op(inner, Path.Op.DIFFERENCE); flareBand.op(o, Path.Op.UNION)
        }
        t.addRect(JeepShape.REAR_X + r0, 138f, JeepShape.FRONT_X - r0, 152f, Path.Direction.CW)   // rocker / slider
        t.addRect(484f, 104f, 512f, 152f, Path.Direction.CW)                     // front bumper
        t.addRect(0f, 122f, JeepShape.REAR_X - r0 * 0.9f, 152f, Path.Direction.CW)   // rear bumper
        // flares stick out a little past the body outline like the real ones, so only the hardtop and the rest are cut to it
        val res = Path(); res.op(t, outer, Path.Op.INTERSECT); res.op(flareBand, Path.Op.UNION)
        res.op(arches, Path.Op.DIFFERENCE); res
    }
    /** the wheel arches: circles a bit larger than the tyres */
    private val arches: Path by lazy {
        Path().apply { for (cx in floatArrayOf(JeepShape.REAR_X, JeepShape.FRONT_X)) addCircle(cx, TYRE_Y, TYRE_R * ARCH_K, Path.Direction.CW) }
    }
    /** the body with the arches cut wider (the mask's own small wheels go with them) */
    private val bodyCut: Path by lazy { Path().apply { op(outer, arches, Path.Op.DIFFERENCE) } }
    /** contour groups: the outer body, the window glass, and the small inner lines (door seams, handles) */
    private fun contourPath(sel: (FloatArray) -> Boolean) = Path().apply {
        for (c in JeepShape.CONTOURS) if (sel(c)) { moveTo(c[0], c[1]); var i = 2; while (i < c.size) { lineTo(c[i], c[i + 1]); i += 2 }; close() }
    }
    private fun bbox(c: FloatArray): FloatArray {
        var x0 = 1e9f; var x1 = -1e9f; var y0 = 1e9f; var y1 = -1e9f; var i = 0
        while (i < c.size) { x0 = minOf(x0, c[i]); x1 = maxOf(x1, c[i]); y0 = minOf(y0, c[i + 1]); y1 = maxOf(y1, c[i + 1]); i += 2 }
        return floatArrayOf(x0, y0, x1, y1)
    }
    private fun isGlass(b: FloatArray) = b[3] < 72f && b[2] - b[0] > 40f
    private val outer: Path by lazy { contourPath { bbox(it).let { b -> b[2] - b[0] > JeepShape.W * 0.8f } } }
    private val glass: Path by lazy { contourPath { isGlass(bbox(it)) } }
    private val seams: Path by lazy { contourPath { bbox(it).let { b -> b[2] - b[0] < JeepShape.W * 0.8f && !isGlass(b) && b[3] < 180f } } }
    // wheels in the mask (px): centres and radius, measured from the 512 px reference (tools/suv_vector.py)
    private const val REAR_X = JeepShape.REAR_X; private const val FRONT_X = JeepShape.FRONT_X
    private const val WHEEL_Y = JeepShape.WHEEL_Y; private const val WHEEL_R = JeepShape.WHEEL_R
    private const val TYRE_K = 1.12f   // owner: bigger wheels; the arches are cut wider to keep the gap (bodyCut)
    private const val ARCH_K = 1.24f   // owner: more room between the arch and the tyre
    private val TYRE_R = JeepShape.WHEEL_R * TYRE_K
    private val TYRE_Y = JeepShape.PIVOT_Y - TYRE_R   // still on the ground
    private const val MW = JeepShape.W / 128f   // the old 128 px mask -> this one (lamp positions below)

    fun draw(c: Canvas, preset: Int, w: Float, h: Float, dim: Boolean = false, time: Float = 0f) {
        t = time
        c.save(); c.clipRect(0f, 0f, w, h)
        when (preset) {
            0 -> highway(c, w, h)
            1 -> rough(c, w, h)
            2 -> gravel(c, w, h)
            3 -> trail(c, w, h)
            4 -> rock(c, w, h)
            5 -> sand(c, w, h)
            6 -> recovery(c, w, h)
            else -> custom(c, w, h)
        }
        p.color = Color.BLACK; p.shader = RadialGradient(w / 2, h * 0.45f, w * 0.75f, Color.TRANSPARENT, 0x66000000, Shader.TileMode.CLAMP)
        c.drawRect(0f, 0f, w, h, p); p.shader = null
        if (dim) { p.color = 0x88000000.toInt(); c.drawRect(0f, 0f, w, h, p) }
        c.restore()
    }

    // ---------- world helpers ----------
    private fun sky(c: Canvas, w: Float, h: Float, top: Int, bottom: Int, until: Float = 1f) {
        p.color = Color.BLACK; p.shader = LinearGradient(0f, 0f, 0f, h * until, top, bottom, Shader.TileMode.CLAMP)
        c.drawRect(0f, 0f, w, h, p); p.shader = null
    }
    /** a layer from y(x) down to the bottom, gradient top -> bottom colour */
    private fun band(c: Canvas, w: Float, h: Float, y: (Float) -> Float, top: Int, bottom: Int) {
        path.reset(); path.moveTo(0f, h)
        var x = 0f; val step = w / 90; var minY = h
        while (x <= w + step) { val yy = y(x); path.lineTo(x, yy); if (yy < minY) minY = yy; x += step }
        path.lineTo(w, h); path.close()
        p.color = Color.BLACK; p.shader = LinearGradient(0f, minY, 0f, h, top, bottom, Shader.TileMode.CLAMP)
        c.drawPath(path, p); p.shader = null
    }
    private fun gauss(x: Float, at: Float, width: Float) = exp(-((x - at) / width) * ((x - at) / width))
    /** a feature repeating every `period` along the world: offset from the nearest copy */
    private fun rep(x: Float, period: Float, at: Float): Float { val u = x - at; return u - period * floor(u / period + 0.5f) }
    private fun sun(c: Canvas, x: Float, y: Float, rad: Float, col: Int) {
        val pulse = 1f + 0.04f * sin(t * 0.8f)
        p.color = Color.BLACK; p.shader = RadialGradient(x, y, rad * 3.2f * pulse, (col and 0x00FFFFFF) or 0x66000000, Color.TRANSPARENT, Shader.TileMode.CLAMP)
        c.drawCircle(x, y, rad * 3.2f * pulse, p); p.shader = null
        p.color = col; c.drawCircle(x, y, rad, p)
    }
    private fun fir(c: Canvas, x: Float, base: Float, hgt: Float, col: Int) {
        p.color = col
        for (k in 0 until 3) {
            val tt = base - hgt * (0.18f + 0.27f * k); val wd = hgt * (0.34f - 0.08f * k)
            path.reset(); path.moveTo(x, tt - hgt * 0.34f); path.lineTo(x + wd, tt + hgt * 0.08f); path.lineTo(x - wd, tt + hgt * 0.08f); path.close()
            c.drawPath(path, p)
        }
        c.drawRect(x - hgt * 0.03f, base - hgt * 0.16f, x + hgt * 0.03f, base, p)
    }
    /** objects every `gap` px of the world, scrolled by `off`: screen x and index of each copy */
    private inline fun every(w: Float, gap: Float, off: Float, f: (x: Float, k: Int) -> Unit) {
        var k = floor(off / gap).toInt() - 1
        while (true) { val x = k * gap - off; if (x > w + gap) break; if (x > -gap) f(x, k); k++ }
    }
    private fun haze(c: Canvas, w: Float, top: Float, bottom: Float, col: Int) {
        p.color = Color.BLACK; p.shader = LinearGradient(0f, top, 0f, bottom, Color.TRANSPARENT, (col and 0x00FFFFFF) or 0x99000000.toInt(), Shader.TileMode.CLAMP)
        c.drawRect(0f, top, w, bottom, p); p.shader = null
    }
    private fun clouds(c: Canvas, w: Float, h: Float, y: Float, col: Int, n: Int, size: Float, speed: Float) {
        p.maskFilter = BlurMaskFilter(h * 0.03f, BlurMaskFilter.Blur.NORMAL); p.color = col
        for (i in 0 until n) {
            val x = (((i * 0.37f + 0.1f) * w - t * speed * w * (1 + i % 3) * 0.5f) % (w * 1.3f) + w * 1.3f) % (w * 1.3f) - w * 0.15f
            val yy = y + h * 0.05f * sin(i * 1.7f); val s2 = size * (0.7f + 0.3f * (i % 3))
            r.set(x - s2, yy - s2 * 0.25f, x + s2, yy + s2 * 0.25f); c.drawOval(r, p)
            r.set(x - s2 * 0.5f, yy - s2 * 0.45f, x + s2 * 0.3f, yy + s2 * 0.1f); c.drawOval(r, p)
        }
        p.maskFilter = null
    }
    private fun stars(c: Canvas, w: Float, h: Float) {
        val rnd = java.util.Random(11)
        repeat(60) {
            val sx = rnd.nextFloat() * w; val sy = rnd.nextFloat() * h * 0.4f
            val tw = 0.5f + 0.5f * sin(t * 1.5f + it)
            p.color = (((90 + 120 * tw * (1 - sy / (h * 0.4f))).toInt().coerceIn(0, 255)) shl 24) or 0xFFFFFF
            c.drawCircle(sx, sy, h * 0.004f * (1 + rnd.nextFloat()), p)
        }
    }

    // ---------- the Jeep ----------
    private class Pose(val x: Float, val y: Float, val ang: Float, val z: Float)
    /** stand the Jeep on the ground: rear wheel at screen x `xr`, both wheels on ground(x), plus a small body bounce */
    private fun pose(xr: Float, len: Float, ground: (Float) -> Float, bounce: Float = 0f): Pose {
        val z = len / JeepShape.W
        val wb = (FRONT_X - REAR_X) * z
        var ang = 0f
        repeat(2) {   // the front wheel's x shifts a little with the pitch
            val yr = ground(xr); val xf = xr + wb * cos(Math.toRadians(ang.toDouble())).toFloat(); val yf = ground(xf)
            ang = Math.toDegrees(atan2((yf - yr).toDouble(), (xf - xr).toDouble())).toFloat()
        }
        // the mask's pivot is the ground under the rear wheel; move from REAR_X to it along the body
        val a = Math.toRadians(ang.toDouble())
        val dxm = (JeepShape.PIVOT_X - REAR_X) * z
        val px = xr + (dxm * cos(a)).toFloat(); val py = ground(xr) + (dxm * sin(a)).toFloat() + bounce
        return Pose(px, py, ang, z)
    }
    private fun maskPt(ps: Pose, mx: Float, my: Float): Pair<Float, Float> {
        val a = Math.toRadians(ps.ang.toDouble())
        val dx = (mx - JeepShape.PIVOT_X) * ps.z; val dy = (my - JeepShape.PIVOT_Y) * ps.z
        return Pair(ps.x + (dx * cos(a) - dy * sin(a)).toFloat(), ps.y + (dx * sin(a) + dy * cos(a)).toFloat())
    }
    private val rubiconFont = android.graphics.Typeface.create(android.graphics.Typeface.create("sans-serif-condensed", android.graphics.Typeface.BOLD), 900, false)
    private fun mix(a: Int, b: Int, k: Float): Int {
        fun ch(sh: Int) = (((a shr sh) and 0xFF) * (1 - k) + ((b shr sh) and 0xFF) * k).toInt().coerceIn(0, 255)
        return (0xFF shl 24) or (ch(16) shl 16) or (ch(8) shl 8) or ch(0)
    }
    private fun jeep(c: Canvas, ps: Pose, body: Int, shade: Int, lights: Boolean = false, wheelTurn: Float, sunk: Float = 0f) {
        val len = JeepShape.W * ps.z
        if (lights) {
            val (hx, hy) = maskPt(ps, 124f * MW, 24f * MW)
            c.save(); c.translate(hx, hy); c.rotate(ps.ang + 4f)
            path.reset(); path.moveTo(0f, -len * 0.015f); path.lineTo(len * 0.9f, -len * 0.09f); path.lineTo(len * 0.9f, len * 0.12f); path.lineTo(0f, len * 0.015f); path.close()
            p.color = Color.BLACK; p.shader = LinearGradient(0f, 0f, len * 0.9f, 0f, 0x66FFF2C8, Color.TRANSPARENT, Shader.TileMode.CLAMP)
            c.drawPath(path, p); p.shader = null; c.restore()
        }
        // contact shadow
        p.color = 0x55000000; p.maskFilter = BlurMaskFilter(len * 0.03f, BlurMaskFilter.Blur.NORMAL)
        val (sx0, sy0) = maskPt(ps, REAR_X - WHEEL_R * 1.3f, JeepShape.H); val (sx1, sy1) = maskPt(ps, FRONT_X + WHEEL_R * 1.3f, JeepShape.H)
        r.set(sx0, minOf(sy0, sy1) - len * 0.012f, sx1, maxOf(sy0, sy1) + len * 0.025f); c.drawOval(r, p); p.maskFilter = null
        c.save(); c.translate(ps.x, ps.y + sunk); c.rotate(ps.ang); c.scale(ps.z, ps.z); c.translate(-JeepShape.PIVOT_X, -JeepShape.PIVOT_Y)
        // the owner's colour: Sting Gray, a warm grey, lit from above; the scene tints the highlight and the shadow a little
        val reflection = Color.red(body) < 0x80
        val top = if (reflection) body else mix(0xFFB4B5AD.toInt(), body, 0.25f)
        val mid = if (reflection) body else 0xFF80817A.toInt()
        val bot = if (reflection) shade else mix(0xFF45464A.toInt(), shade, 0.3f)
        p.color = Color.BLACK
        p.shader = LinearGradient(0f, 0f, 0f, 152f, intArrayOf(top, mid, mid, bot), floatArrayOf(0f, 0.3f, 0.62f, 1f), Shader.TileMode.CLAMP)
        c.drawPath(bodyCut, p); p.shader = null
        // tinted glass with a soft sky reflection
        p.shader = LinearGradient(0f, 10f, 0f, 62f, 0xFF4C5761.toInt(), 0xFF15191D.toInt(), Shader.TileMode.CLAMP)
        c.drawPath(glass, p); p.shader = null
        c.save(); c.clipPath(glass); p.color = 0x22FFFFFF
        path.reset(); path.moveTo(60f, 62f); path.lineTo(110f, 10f); path.lineTo(140f, 10f); path.lineTo(90f, 62f); path.close(); c.drawPath(path, p)
        path.offset(170f, 0f); c.drawPath(path, p); c.restore()
        // door seams and handles
        p.color = 0xFF2A2B2E.toInt(); c.drawPath(seams, p)
        // black details: door hinges, the mirror, the window rubber at the belt line
        p.color = 0xFF1A1B1D.toInt()
        for ((hx, hy) in listOf(248f to 80f, 248f to 116f, 338f to 82f, 338f to 118f)) { r.set(hx, hy, hx + 8f, hy + 11f); c.drawRoundRect(r, 2f, 2f, p) }
        r.set(324f, 44f, 346f, 64f); c.drawRoundRect(r, 5f, 5f, p)
        c.drawRect(50f, 62f, 326f, 65f, p)

        // JL trim: satin black with a faint sheen from above
        p.color = Color.BLACK
        p.shader = LinearGradient(0f, 0f, 0f, JeepShape.H, 0xFF3A3B40.toInt(), 0xFF141416.toInt(), Shader.TileMode.CLAMP)
        c.drawPath(trim, p); p.shader = null
        // red RUBICON on the hood side, above the front flare (owner's photo)
        p.typeface = rubiconFont; p.textSize = 20f; p.color = 0xFFB3262A.toInt(); p.letterSpacing = 0.1f
        c.drawText("RUBICON", 352f, 82f, p); p.letterSpacing = 0f
        // the spare wheel on the tailgate, seen on its tread: a dark rounded tyre with grooves and a soft highlight
        r.set(1f, 40f, 37f, 136f)
        p.color = Color.BLACK
        p.shader = LinearGradient(1f, 0f, 37f, 0f, intArrayOf(0xFF0F0F11.toInt(), 0xFF3A3B40.toInt(), 0xFF141416.toInt()), floatArrayOf(0f, 0.45f, 1f), Shader.TileMode.CLAMP)
        c.drawRoundRect(r, 15f, 22f, p); p.shader = null
        p.color = 0xFF08080A.toInt(); var gy = 50f
        while (gy < 128f) { c.drawRect(5f, gy, 33f, gy + 2.2f, p); gy += 7f }
        p.color = 0xFF1C1D20.toInt(); c.drawRect(36f, 70f, 42f, 108f, p)   // carrier bracket
        // a thin dark outline, so the body stands out on light scenes (sand, dusk)
        p.style = Paint.Style.STROKE; p.strokeWidth = 1.1f * MW; p.color = 0x66000000; c.drawPath(bodyCut, p); p.style = Paint.Style.FILL
        // wheels (owner: bigger, a lifted JL on 35s): a fat dark tyre with tread blocks, a satin rim with five spokes.
        // The tyre is 1.3x the mask wheel; its centre is lifted so it still stands on the ground.
        val tr = TYRE_R; val cy = TYRE_Y
        // dark wheel wells, so the arch is not see-through (owner's photo)
        c.save(); c.clipRect(0f, 0f, JeepShape.W, JeepShape.H); p.color = 0xFF0E0E10.toInt()
        for (wx in floatArrayOf(REAR_X, FRONT_X)) c.drawCircle(wx, cy, tr * ARCH_K, p)
        c.restore()
        for (wx in floatArrayOf(REAR_X, FRONT_X)) {
            p.color = 0xFF1B1C1F.toInt(); c.drawCircle(wx, cy, tr, p)
            p.color = 0xFF2E3035.toInt()
            for (k in 0 until 22) {   // mud-terrain tread: big staggered blocks on the shoulder, turning
                val a = wheelTurn + k * 0.2856f
                c.save(); c.rotate(Math.toDegrees(a.toDouble()).toFloat(), wx, cy)
                val inset = if (k % 2 == 0) 0.8f else 0.84f
                c.drawRect(wx + tr * inset, cy - tr * 0.07f, wx + tr * 1.01f, cy + tr * 0.07f, p); c.restore()
            }
            p.color = 0xFF111214.toInt(); c.drawCircle(wx, cy, tr * 0.68f, p)          // sidewall shadow
            // Rubicon wheel: gloss black, five split spokes with a machined silver face
            p.color = 0xFF151518.toInt(); c.drawCircle(wx, cy, tr * 0.6f, p)
            for (k in 0 until 5) {
                val a0 = wheelTurn + k * 1.2566f
                c.save(); c.rotate(Math.toDegrees(a0.toDouble()).toFloat(), wx, cy)
                for (side in floatArrayOf(-1f, 1f)) {   // a split spoke: two blades with a silver face
                    path.reset()
                    path.moveTo(wx + tr * 0.14f, cy + side * tr * 0.03f); path.lineTo(wx + tr * 0.56f, cy + side * tr * 0.05f)
                    path.lineTo(wx + tr * 0.56f, cy + side * tr * 0.16f); path.lineTo(wx + tr * 0.14f, cy + side * tr * 0.09f); path.close()
                    p.color = 0xFFCDD1D6.toInt(); c.drawPath(path, p)
                }
                c.restore()
            }
            p.style = Paint.Style.STROKE; p.strokeWidth = tr * 0.05f; p.color = 0xFF0C0C0E.toInt(); c.drawCircle(wx, cy, tr * 0.58f, p); p.style = Paint.Style.FILL
            p.color = 0xFF1F2023.toInt(); c.drawCircle(wx, cy, tr * 0.16f, p)
            p.color = 0xFF8A8E95.toInt(); c.drawCircle(wx, cy, tr * 0.06f, p)
        }
        c.restore()
        if (lights) {
            val (hx, hy) = maskPt(ps, 124f * MW, 24f * MW)
            p.color = Color.BLACK; p.shader = RadialGradient(hx, hy, len * 0.06f, 0xFFFFF6D8.toInt(), Color.TRANSPARENT, Shader.TileMode.CLAMP); c.drawCircle(hx, hy, len * 0.06f, p)
            val (tx, ty) = maskPt(ps, 10f * MW, 27f * MW)
            p.color = Color.BLACK; p.shader = RadialGradient(tx, ty, len * 0.04f, 0xFFFF3B30.toInt(), Color.TRANSPARENT, Shader.TileMode.CLAMP); c.drawCircle(tx, ty, len * 0.04f, p)
            p.shader = null
        }
    }
    /** wheel turn angle for a ground speed (px per second) and the wheel radius on screen */
    private fun turn(speedPx: Float, z: Float) = t * speedPx / (TYRE_R * z)
    /** dust behind the rear wheel */
    private fun dust(c: Canvas, ps: Pose, col: Int) {
        val (x, y) = maskPt(ps, REAR_X - WHEEL_R * 0.5f, JeepShape.H - 2f); val len = JeepShape.W * ps.z
        for (i in 0 until 16) {
            val ph = ((t * 0.8f + i / 16f) % 1f)
            val dx = -ph * len * 0.55f; val dy = -ph * len * 0.14f + sin(i * 2.1f) * len * 0.02f
            val rr = len * (0.02f + 0.07f * ph)
            val a = ((1f - ph) * 0.45f * 255).toInt().coerceIn(0, 255)
            p.color = (col and 0x00FFFFFF) or (a shl 24)
            p.maskFilter = BlurMaskFilter(rr * 0.6f, BlurMaskFilter.Blur.NORMAL)
            c.drawCircle(x + dx, y + dy, rr, p)
        }
        p.maskFilter = null
    }

    // ---------- scenes ----------
    private fun highway(c: Canvas, w: Float, h: Float) {   // dusk, the city slides by, lane dashes rush under the Jeep
        val speed = w * 0.6f; val off = t * speed
        sky(c, w, h, 0xFF0B1B3A.toInt(), 0xFFF08A3C.toInt(), 0.78f)
        stars(c, w, h)
        sun(c, w * 0.78f, h * 0.6f, h * 0.07f, 0xFFFFC46B.toInt())
        val g = h * 0.74f
        val hs = floatArrayOf(.28f, .4f, .22f, .5f, .33f, .45f, .26f, .38f, .55f, .3f, .42f, .24f)
        every(w, w * 0.11f, off * 0.15f) { x, k ->   // skyline, slow
            val i = ((k % hs.size) + hs.size) % hs.size
            val bw = w * (0.06f + 0.025f * (i % 3)); val bh = h * hs[i] * 0.8f
            p.color = 0xFF1A2236.toInt(); c.drawRect(x, g - bh, x + bw, g, p)
            p.color = 0xAAFFD68A.toInt()
            var wy = g - bh + h * 0.03f
            while (wy < g - h * 0.03f) { var wx = x + bw * 0.2f; var q = 0; while (wx < x + bw * 0.8f) { if ((q + i + (wy / h * 40).toInt()) % 3 != 0) c.drawRect(wx, wy, wx + w * 0.006f, wy + h * 0.012f, p); wx += bw * 0.22f; q++ }; wy += h * 0.04f }
        }
        haze(c, w, g - h * 0.25f, g, 0xFFF08A3C.toInt())
        p.color = Color.BLACK; p.shader = LinearGradient(0f, g, 0f, h, 0xFF2B2D33.toInt(), 0xFF111216.toInt(), Shader.TileMode.CLAMP); c.drawRect(0f, g, w, h, p); p.shader = null
        p.color = 0xFF5A5D66.toInt(); c.drawRect(0f, g, w, g + h * 0.012f, p)
        p.color = 0xEEFFFFFF.toInt()
        every(w, w * 0.18f, off) { x, _ -> c.drawRect(x, g + h * 0.15f, x + w * 0.09f, g + h * 0.17f, p) }
        val road = g + h * 0.075f
        val ps = pose(w * 0.3f, w * 0.48f, { road }, bounce = h * 0.003f * sin(t * 17f))
        jeep(c, ps, 0xFFE8E4F0.toInt(), 0xFF5D6378.toInt(), lights = true, wheelTurn = turn(speed, ps.z))
    }

    private fun rough(c: Canvas, w: Float, h: Float) {   // overcast, broken asphalt with bumps and potholes
        val speed = w * 0.2f; val off = t * speed
        sky(c, w, h, 0xFF2E3D4F.toInt(), 0xFF9FB0BE.toInt(), 0.75f)
        clouds(c, w, h, h * 0.18f, 0x55E6EEF5, 5, w * 0.16f, 0.01f)
        band(c, w, h, { x -> h * (0.55f - 0.08f * sin((x + off * 0.1f) / w * 5f)) }, 0xFF4F6372.toInt(), 0xFF35454F.toInt())
        haze(c, w, h * 0.45f, h * 0.66f, 0xFF9FB0BE.toInt())
        band(c, w, h, { x -> h * (0.66f - 0.05f * sin((x + off * 0.3f) / w * 7f + 1f)) }, 0xFF3D4D45.toInt(), 0xFF27322C.toInt())
        val period = w * 0.9f
        val g = { x: Float -> val u = x + off; h * 0.8f - h * 0.06f * gauss(rep(u, period, 0f), 0f, w * 0.05f) + h * 0.03f * gauss(rep(u, period, period * 0.5f), 0f, w * 0.03f) }
        band(c, w, h, g, 0xFF3A3C42.toInt(), 0xFF17181C.toInt())
        p.color = 0xFF0E0F12.toInt(); p.style = Paint.Style.STROKE; p.strokeWidth = h * 0.008f
        every(w, w * 0.45f, off) { x, _ -> path.reset(); path.moveTo(x, h * 0.85f); path.lineTo(x + w * 0.03f, h * 0.89f); path.lineTo(x + w * 0.07f, h * 0.87f); path.lineTo(x + w * 0.1f, h * 0.92f); c.drawPath(path, p) }
        p.style = Paint.Style.FILL
        val ps = pose(w * 0.2f, w * 0.46f, g)
        jeep(c, ps, 0xFFEDF1F5.toInt(), 0xFF7D8792.toInt(), wheelTurn = turn(speed, ps.z))
    }

    private fun gravel(c: Canvas, w: Float, h: Float) {   // afternoon, pines, a dirt road, stones and dust
        val speed = w * 0.3f; val off = t * speed
        sky(c, w, h, 0xFF4F86BF.toInt(), 0xFFF1D7A8.toInt(), 0.8f)
        sun(c, w * 0.15f, h * 0.22f, h * 0.06f, 0xFFFFF1C4.toInt())
        clouds(c, w, h, h * 0.16f, 0x88FFFFFF.toInt(), 4, w * 0.13f, 0.012f)
        band(c, w, h, { x -> h * (0.6f - 0.07f * sin((x + off * 0.1f) / w * 4f + 0.5f)) }, 0xFF6E8A6A.toInt(), 0xFF4D664B.toInt())
        haze(c, w, h * 0.5f, h * 0.68f, 0xFFF1D7A8.toInt())
        every(w, w * 0.07f, off * 0.35f) { x, k -> fir(c, x, h * 0.72f, h * (0.24f + 0.06f * (((k % 3) + 3) % 3)), 0xFF2E4A33.toInt()) }
        val g = { x: Float -> h * 0.8f + h * 0.012f * sin((x + off) / w * 13f) }
        band(c, w, h, g, 0xFFB08A60.toInt(), 0xFF6E5236.toInt())
        val rnd = java.util.Random(7)
        repeat(70) {
            val fx = rnd.nextFloat(); val fy = rnd.nextFloat(); val fr = rnd.nextFloat()
            val sx = ((fx * w * 1.5f - off) % (w * 1.5f) + w * 1.5f) % (w * 1.5f) - w * 0.25f
            val sy = h * 0.83f + fy * h * 0.17f
            p.color = if (fr > 0.5f) 0xFFD9C4A3.toInt() else 0xFF8C7458.toInt(); c.drawCircle(sx, sy, h * (0.006f + fr * 0.012f), p)
        }
        val ps = pose(w * 0.18f, w * 0.47f, g)
        dust(c, ps, 0xFFD9C4A3.toInt())
        jeep(c, ps, 0xFFFFF8EE.toInt(), 0xFF9C8C78.toInt(), wheelTurn = turn(speed, ps.z))
    }

    private fun trail(c: Canvas, w: Float, h: Float) {   // forest at dusk, layers of firs, a wavy trail
        val speed = w * 0.18f; val off = t * speed
        sky(c, w, h, 0xFF1C3A2E.toInt(), 0xFFB9D49A.toInt(), 0.7f)
        every(w, w * 0.08f, off * 0.2f) { x, _ -> fir(c, x, h * 0.66f, h * 0.34f, 0xFF3F6B4C.toInt()) }
        haze(c, w, h * 0.35f, h * 0.68f, 0xFFB9D49A.toInt())
        every(w, w * 0.11f, off * 0.5f) { x, _ -> fir(c, x, h * 0.74f, h * 0.48f, 0xFF244634.toInt()) }
        val g = { x: Float -> h * 0.8f + h * 0.045f * sin((x + off) / w * 8f) + h * 0.015f * sin((x + off) / w * 23f) }
        band(c, w, h, g, 0xFF6B4E33.toInt(), 0xFF3A2A1C.toInt())
        p.color = 0x553A2A1C
        every(w, w * 0.12f, off) { x, _ -> c.drawRect(x, g(x) + h * 0.09f, x + w * 0.05f, g(x) + h * 0.105f, p) }
        val ps = pose(w * 0.22f, w * 0.46f, g)
        jeep(c, ps, 0xFFEFF5EA.toInt(), 0xFF5F7562.toInt(), lights = true, wheelTurn = turn(speed, ps.z))
    }

    private fun rock(c: Canvas, w: Float, h: Float) {   // alpine: the Jeep crawls slowly over boulders
        val speed = w * 0.07f; val off = t * speed
        sky(c, w, h, 0xFF2D63A0.toInt(), 0xFFD6E9F7.toInt(), 0.7f)
        fun peak(cx: Float, top: Float, half: Float, col: Int) {
            path.reset(); path.moveTo(cx - half, h * 0.78f); path.lineTo(cx, top); path.lineTo(cx + half, h * 0.78f); path.close()
            p.color = Color.BLACK; p.shader = LinearGradient(cx - half, top, cx + half, h * 0.78f, col, 0xFF2A3340.toInt(), Shader.TileMode.CLAMP)
            c.drawPath(path, p); p.shader = null
            path.reset(); path.moveTo(cx, top); path.lineTo(cx + half * 0.3f, top + (h * 0.78f - top) * 0.3f)
            path.lineTo(cx + half * 0.08f, top + (h * 0.78f - top) * 0.24f); path.lineTo(cx - half * 0.1f, top + (h * 0.78f - top) * 0.33f)
            path.lineTo(cx - half * 0.3f, top + (h * 0.78f - top) * 0.3f); path.close()
            p.color = Color.WHITE; c.drawPath(path, p)
        }
        every(w, w * 0.55f, off * 0.05f) { x, k -> peak(x, h * (0.12f + 0.08f * (((k % 2) + 2) % 2)), w * 0.3f, 0xFF7C8FA8.toInt()) }
        haze(c, w, h * 0.3f, h * 0.78f, 0xFFD6E9F7.toInt())
        clouds(c, w, h, h * 0.3f, 0x66FFFFFF, 3, w * 0.12f, 0.008f)
        // the ground: a boulder every 0.8 w, the wheels climb it one after the other
        val period = w * 0.8f
        val g = { x: Float ->
            val u = x + off
            val d = rep(u, period, 0f)
            val k = (d - w * 0.02f) / (w * 0.18f)
            val boulder = h * 0.3f * (1f - k * k).coerceAtLeast(0f)
            h * 0.86f - boulder - h * 0.04f * gauss(rep(u, period, period * 0.55f), 0f, w * 0.05f)
        }
        band(c, w, h, g, 0xFF8E8A84.toInt(), 0xFF3B3935.toInt())
        p.color = 0x22FFFFFF
        every(w, period, off) { x, _ -> path.reset(); path.moveTo(x - w * 0.12f, g(x - w * 0.12f)); path.lineTo(x, g(x)); path.lineTo(x + w * 0.02f, g(x) + h * 0.06f); path.close(); c.drawPath(path, p) }
        val ps = pose(w * 0.14f, w * 0.44f, g)
        jeep(c, ps, 0xFFF2F7FF.toInt(), 0xFF6C7C92.toInt(), wheelTurn = turn(speed, ps.z))
    }

    private fun sand(c: Canvas, w: Float, h: Float) {   // desert noon: rolling over dunes, dust behind
        val speed = w * 0.22f; val off = t * speed
        sky(c, w, h, 0xFFF29A3F.toInt(), 0xFFFDE7B8.toInt(), 0.65f)
        sun(c, w * 0.8f, h * 0.25f, h * 0.11f, 0xFFFFF4D6.toInt())
        band(c, w, h, { x -> val u = x + off * 0.15f; h * (0.6f - 0.1f * gauss(rep(u, w * 1.1f, 0f), 0f, w * 0.2f) - 0.06f * gauss(rep(u, w * 0.7f, w * 0.3f), 0f, w * 0.12f)) }, 0xFFE9B36F.toInt(), 0xFFC98942.toInt())
        val g = { x: Float -> val u = x + off; h * 0.84f - h * 0.12f * (0.5f + 0.5f * sin(u / w * 3.4f)) - h * 0.03f * sin(u / w * 9f) }
        band(c, w, h, g, 0xFFF0C27E.toInt(), 0xFFB97734.toInt())
        p.color = 0x22000000; p.style = Paint.Style.STROKE; p.strokeWidth = h * 0.006f
        for (i in 0 until 4) { path.reset(); var x = 0f; path.moveTo(0f, g(0f) + h * (0.04f + i * 0.03f)); while (x < w) { path.lineTo(x, g(x) + h * (0.04f + i * 0.03f) + h * 0.006f * sin((x + off) / w * 40)); x += w / 40 }; c.drawPath(path, p) }
        p.style = Paint.Style.FILL
        val ps = pose(w * 0.18f, w * 0.45f, g)
        dust(c, ps, 0xFFF3CD8F.toInt())
        jeep(c, ps, 0xFFFFFFFF.toInt(), 0xFF8E6440.toInt(), wheelTurn = turn(speed, ps.z))
    }

    private fun recovery(c: Canvas, w: Float, h: Float) {   // stuck: the wheels spin in the sand and throw it back
        sky(c, w, h, 0xFF7A2E3B.toInt(), 0xFFF5A35C.toInt(), 0.7f)
        sun(c, w * 0.2f, h * 0.62f, h * 0.09f, 0xFFFFC27A.toInt())
        band(c, w, h, { x -> h * (0.66f - 0.06f * sin(x / w * 4f)) }, 0xFFD18A4C.toInt(), 0xFFA8652F.toInt())
        val shake = h * 0.004f * sin(t * 23f)   // the body shakes while the wheels dig
        val ps = pose(w * 0.24f, w * 0.46f, { h * 0.8f }, bounce = shake)
        val sunk = h * 0.06f
        jeep(c, ps, 0xFFFFFFFF.toInt(), 0xFF7E4A38.toInt(), lights = true, wheelTurn = t * 14f, sunk = sunk)
        // the sand the wheels sit in, piled a little higher at each wheel
        val (rx, _) = maskPt(ps, REAR_X, WHEEL_Y); val (fx, _) = maskPt(ps, FRONT_X, WHEEL_Y)
        band(c, w, h, { x -> h * (0.84f - 0.015f * sin(x / w * 11f)) - h * 0.03f * (gauss(x, rx, w * 0.07f) + gauss(x, fx, w * 0.07f)) }, 0xFFE3A15B.toInt(), 0xFFB06A2E.toInt())
        // sand thrown back and up from both wheels, falling again
        for ((wx, seed) in listOf(rx to 0.3f, fx to 0.7f)) {
            for (i in 0 until 28) {
                val ph = ((t * 2.2f + i / 28f + seed) % 1f)
                val vx = -(0.3f + 0.3f * ((i * 37) % 10) / 10f) * w; val vy = -(0.5f + 0.35f * ((i * 53) % 10) / 10f) * h
                val x = wx - w * 0.02f + vx * ph * 0.5f; val y = h * 0.82f + vy * ph * 0.5f + 0.9f * h * ph * ph
                val a = ((1f - ph) * 230).toInt().coerceIn(0, 255)
                p.color = (a shl 24) or 0xF2C27E; c.drawCircle(x, y, h * (0.005f + 0.005f * ((i * 7) % 3)), p)
            }
        }
        for (k in 0 until 2) {   // recovery boards, ready
            c.save(); c.translate(w * (0.82f + k * 0.07f), h * 0.86f); c.rotate(-18f + k * 8f)
            r.set(-w * 0.025f, -h * 0.42f, w * 0.025f, 0f); p.color = 0xFFFF7A1A.toInt(); c.drawRoundRect(r, w * 0.01f, w * 0.01f, p)
            p.color = 0x55000000; var yy = -h * 0.38f; while (yy < -h * 0.03f) { c.drawRect(-w * 0.015f, yy, w * 0.015f, yy + h * 0.012f, p); yy += h * 0.05f }
            c.restore()
        }
    }

    private fun custom(c: Canvas, w: Float, h: Float) {   // studio: dark backdrop, a floor with a reflection, a gauge
        sky(c, w, h, 0xFF2A2C33.toInt(), 0xFF0E0F12.toInt())
        val sweep = (t * 0.15f) % 1f
        p.color = Color.BLACK; p.shader = RadialGradient(w * (0.1f + 0.8f * sweep), h * 0.82f, w * 0.5f, 0x4430D158, Color.TRANSPARENT, Shader.TileMode.CLAMP)
        c.drawRect(0f, 0f, w, h, p); p.shader = null
        p.color = 0xFF1A1B20.toInt(); c.drawRect(0f, h * 0.8f, w, h, p)
        val ps = pose(w * 0.14f, w * 0.46f, { h * 0.8f })
        c.save(); c.scale(1f, -1f, 0f, h * 0.8f); jeep(c, ps, 0xFF3A3D45.toInt(), 0xFF1A1B20.toInt(), wheelTurn = 0f); c.restore()
        p.color = Color.BLACK; p.shader = LinearGradient(0f, h * 0.8f, 0f, h, 0x991A1B20.toInt(), 0xFF1A1B20.toInt(), Shader.TileMode.CLAMP); c.drawRect(0f, h * 0.8f, w, h, p); p.shader = null
        jeep(c, ps, 0xFFF2F2F7.toInt(), 0xFF6E717C.toInt(), wheelTurn = 0f)
        val gx = w * 0.82f; val gy = h * 0.35f; val gr = h * 0.2f
        p.style = Paint.Style.STROKE; p.strokeWidth = gr * 0.12f; p.strokeCap = Paint.Cap.ROUND
        r.set(gx - gr, gy - gr, gx + gr, gy + gr)
        p.color = 0xFF3A3A3C.toInt(); c.drawArc(r, 140f, 260f, false, p)
        val needle = 170f + 20f * sin(t * 0.7f)
        p.color = 0xFF30D158.toInt(); c.drawArc(r, 140f, needle, false, p)
        p.strokeWidth = gr * 0.06f; p.color = Color.WHITE
        val a = Math.toRadians(140.0 + needle)
        c.drawLine(gx, gy, gx + (gr * 0.75f * cos(a)).toFloat(), gy + (gr * 0.75f * sin(a)).toFloat(), p)
        p.style = Paint.Style.FILL; p.strokeCap = Paint.Cap.BUTT; c.drawCircle(gx, gy, gr * 0.1f, p)
    }
}
