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
import kotlin.math.min
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
    // ---------- the owner's Jeep: JeepShape.LAYERS (tools/jl_blueprint.py, read off his photos), drawn as they are ----------
    private const val REAR_X = JeepShape.REAR_X; private const val FRONT_X = JeepShape.FRONT_X
    private const val WHEEL_Y = JeepShape.WHEEL_Y; private const val WHEEL_R = JeepShape.WHEEL_R
    private const val TYRE_R = JeepShape.WHEEL_R            // 33 in tyres, to scale
    private const val TYRE_Y = JeepShape.WHEEL_Y
    private val layerPaths: List<Path> by lazy {
        JeepShape.LAYERS.map { l ->
            Path().apply {
                moveTo(l.pts[0], l.pts[1]); var i = 2
                while (i < l.pts.size) { lineTo(l.pts[i], l.pts[i + 1]); i += 2 }
                if (l.kind == 0) close()
            }
        }
    }

    // owner: while a run pumps or vents, the Jeep stands (it does in reality): the world eases to a stop in 0.6 s, the
    // amber marker blinks once a second and an arc round each tyre fills with the progress. td = seconds of driving.
    private var td = 0f; private var lastT = 0f; private var moving = 1f
    private var runProg = -1f; private var runCol = 0
    fun draw(c: Canvas, preset: Int, w: Float, h: Float, dim: Boolean = false, time: Float = 0f, running: Boolean = false, progress: Float = -1f, accent: Int = 0) {
        val dt = (time - lastT).coerceIn(0f, 0.1f); lastT = time
        moving += ((if (running) 0f else 1f) - moving).coerceIn(-dt / 0.6f, dt / 0.6f)
        td += dt * moving
        runProg = if (running) progress else -1f; runCol = accent
        t = time
        kw = min(w, h / 0.62f)   // the Jeep's frame: a card wider than 0.62 (the unfolded Fold) keeps the car to its height
        cx = w / 2
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
        p.color = Color.BLACK; p.shader = RadialGradient(w / 2, h * 0.45f, w * 0.75f, Color.TRANSPARENT, 0x40000000, Shader.TileMode.CLAMP)
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
    private fun fir(c: Canvas, x0: Float, base: Float, hgt0: Float, col: Int, k: Int = 0) {
        // advisor: vary height +-25 % and place by a hash of the index; the right half of each tier in shade
        val hsh = ((k * 2654435761L) ushr 8).toInt() and 0xFFFF
        val hgt = hgt0 * (0.75f + 0.5f * (hsh and 0xFF) / 255f); val x = x0 + hgt0 * 0.25f * (((hsh shr 8) and 0xFF) / 255f - 0.5f)
        val dark = mix(col, 0xFF000000.toInt(), 0.18f)
        for (tier in 0 until 3) {
            val tt = base - hgt * (0.18f + 0.27f * tier); val wd = hgt * (0.34f - 0.08f * tier)
            path.reset(); path.moveTo(x, tt - hgt * 0.34f); path.lineTo(x, tt + hgt * 0.08f); path.lineTo(x - wd, tt + hgt * 0.08f); path.close()
            p.color = col; c.drawPath(path, p)
            path.reset(); path.moveTo(x, tt - hgt * 0.34f); path.lineTo(x + wd, tt + hgt * 0.08f); path.lineTo(x, tt + hgt * 0.08f); path.close()
            p.color = dark; c.drawPath(path, p)
        }
        p.color = dark; c.drawRect(x - hgt * 0.03f, base - hgt * 0.16f, x + hgt * 0.03f, base, p)
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
            p.color = (((63 + 84 * tw * (1 - sy / (h * 0.4f))).toInt().coerceIn(0, 255)) shl 24) or 0xFFFFFF
            c.drawCircle(sx, sy, h * 0.004f * (1 + rnd.nextFloat()), p)
        }
    }

    // ---------- the Jeep ----------
    private class Pose(val x: Float, val y: Float, val ang: Float, val z: Float)
    /** stand the Jeep on the ground: rear wheel at screen x `xr`, both wheels on ground(x), plus a small body bounce */
    private var kw = 1f; private var cx = 0f
    /** the Jeep at fraction a of a 0.62 card (rear wheel) and b of its width (length), centred the same way in a wider card */
    private fun at(a: Float, b: Float, ground: (Float) -> Float, bounce: Float = 0f) = pose(cx - (0.5f - a) * kw, b * kw, ground, bounce)
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
    private val decalFont = android.graphics.Typeface.create(android.graphics.Typeface.create("sans-serif", android.graphics.Typeface.BOLD), 900, false)   // wide heavy caps, like the decal
    private val badgeFont = android.graphics.Typeface.create("sans-serif", android.graphics.Typeface.BOLD_ITALIC)
    private fun mix(a: Int, b: Int, k: Float): Int {
        fun ch(sh: Int) = (((a shr sh) and 0xFF) * (1 - k) + ((b shr sh) and 0xFF) * k).toInt().coerceIn(0, 255)
        return (0xFF shl 24) or (ch(16) shl 16) or (ch(8) shl 8) or ch(0)
    }
    private fun jeep(c: Canvas, ps: Pose, body: Int, shade: Int, lights: Boolean = false, wheelTurn: Float, sunk: Float = 0f, spin: Float = 0f) {
        val len = JeepShape.W * ps.z
        val J = JeepShape
        val ghost = Color.red(body) < 0x80          // the floor reflection in CUSTOM: the same car, faint
        if (lights) {   // headlight beam, under the body
            val (hx, hy) = maskPt(ps, J.HEAD_X, J.HEAD_Y)
            c.save(); c.translate(hx, hy); c.rotate(ps.ang + 4f)
            path.reset(); path.moveTo(0f, -len * 0.012f); path.lineTo(len * 0.8f, -len * 0.08f); path.lineTo(len * 0.8f, len * 0.1f); path.lineTo(0f, len * 0.012f); path.close()
            p.color = Color.BLACK; p.shader = LinearGradient(0f, 0f, len * 0.8f, 0f, 0x40FFF2C8, Color.TRANSPARENT, Shader.TileMode.CLAMP)
            c.drawPath(path, p); p.shader = null; c.restore()
        }
        if (!ghost) {   // contact shadow
            p.color = 0x70000000; p.maskFilter = BlurMaskFilter(len * 0.02f, BlurMaskFilter.Blur.NORMAL)
            val (sx0, sy0) = maskPt(ps, REAR_X - WHEEL_R * 1.6f, J.H); val (sx1, sy1) = maskPt(ps, FRONT_X + WHEEL_R * 1.5f, J.H)
            r.set(sx0, minOf(sy0, sy1) - len * 0.01f, sx1, maxOf(sy0, sy1) + len * 0.02f); c.drawOval(r, p); p.maskFilter = null
        }
        c.save(); c.translate(ps.x, ps.y + sunk); c.rotate(ps.ang); c.scale(ps.z, ps.z); c.translate(-J.PIVOT_X, -J.PIVOT_Y)
        if (ghost) c.saveLayerAlpha(0f, 0f, J.W, J.H, 60)
        // the layers of the blueprint, back to front; body-colour parts take a little of the scene's light
        val n = J.LAYERS.size
        for (i in 0 until n) {
            val l = J.LAYERS[i]; val pa = layerPaths[i]
            if (l.kind == 0) {
                val c0 = if (l.tint) mix(l.c0, body, 0.08f) else l.c0
                val c1 = if (l.tint) mix(l.c1, shade, 0.12f) else l.c1
                p.style = Paint.Style.FILL; p.color = Color.BLACK
                if (c0 == c1) { p.color = c0; p.shader = null } else p.shader = LinearGradient(0f, l.y0, 0f, l.y1, c0, c1, Shader.TileMode.CLAMP)
                c.drawPath(pa, p); p.shader = null
                if (l.sw > 0f) { p.style = Paint.Style.STROKE; p.strokeWidth = l.sw; p.strokeJoin = Paint.Join.ROUND; p.color = l.stroke; c.drawPath(pa, p); p.style = Paint.Style.FILL }
            } else {
                p.style = Paint.Style.STROKE; p.strokeWidth = l.sw; p.strokeCap = Paint.Cap.ROUND; p.strokeJoin = Paint.Join.ROUND; p.color = l.stroke
                c.drawPath(pa, p); p.style = Paint.Style.FILL; p.strokeCap = Paint.Cap.BUTT
            }
        }
        // RUBICON along the hood side, red with a dark edge, and the Jeep badge behind the front arch
        p.typeface = decalFont; p.textSize = J.DECAL_H * 1.25f; p.letterSpacing = 0.04f; p.textScaleX = 1f
        p.textScaleX = (J.DECAL_X1 - J.DECAL_X0) / p.measureText("RUBICON")
        p.style = Paint.Style.STROKE; p.strokeWidth = 0.6f; p.color = 0xFF3A0A0D.toInt(); c.drawText("RUBICON", J.DECAL_X0, J.DECAL_Y, p)
        p.style = Paint.Style.FILL; p.color = 0xFFB4232A.toInt(); c.drawText("RUBICON", J.DECAL_X0, J.DECAL_Y, p)
        p.textScaleX = 1f; p.letterSpacing = 0f
        p.typeface = badgeFont; p.textSize = J.BADGE_H * 1.2f; p.color = 0xFF8E1F2A.toInt(); c.drawText("Jeep", J.BADGE_X, J.BADGE_Y, p)
        // wheels
        val tr = TYRE_R; val cy = TYRE_Y
        val fast = spin > 0.35f * 25f
        for (wx in floatArrayOf(REAR_X, FRONT_X)) {
            // mud-terrain tyre: lugs break the outline, a second row on the shoulder
            p.color = 0xFF141517.toInt(); c.drawCircle(wx, cy, tr * 0.975f, p)
            for (k in 0 until 22) {
                c.save(); c.rotate(Math.toDegrees((wheelTurn + k * 0.2856f).toDouble()).toFloat(), wx, cy)
                c.drawRect(wx + tr * 0.93f, cy - tr * 0.07f, wx + tr * 1.0f, cy + tr * 0.07f, p); c.restore()
            }
            p.color = 0xFF232427.toInt()
            for (k in 0 until 22) {
                c.save(); c.rotate(Math.toDegrees((wheelTurn + k * 0.2856f + 0.143f).toDouble()).toFloat(), wx, cy)
                c.drawRect(wx + tr * 0.82f, cy - tr * 0.055f, wx + tr * 0.925f, cy + tr * 0.055f, p); c.restore()
            }
            p.color = 0xFF18191B.toInt(); c.drawCircle(wx, cy, tr * 0.81f, p)                 // sidewall
            p.style = Paint.Style.STROKE; p.strokeWidth = tr * 0.018f; p.color = 0xFF26272A.toInt(); c.drawCircle(wx, cy, tr * 0.72f, p); p.style = Paint.Style.FILL
            // Rubicon wheel (owner's car): black, five wide spokes, each a machined silver frame round a black pocket,
            // a polished ring inside a black lip, five lug nuts round the cap. 17 in wheel in a 33 in tyre.
            p.color = 0xFF060607.toInt(); c.drawCircle(wx, cy, tr * 0.60f, p)
            p.style = Paint.Style.STROKE; p.strokeWidth = tr * 0.022f; p.color = 0xFF8F949B.toInt(); c.drawCircle(wx, cy, tr * 0.565f, p); p.style = Paint.Style.FILL
            p.color = 0xFF0B0B0D.toInt(); c.drawCircle(wx, cy, tr * 0.545f, p)
            for (k in 0 until 5) {
                c.save(); c.rotate(Math.toDegrees((wheelTurn + k * 1.2566f).toDouble()).toFloat(), wx, cy)
                path.reset()
                path.moveTo(wx + tr * 0.13f, cy - tr * 0.085f); path.lineTo(wx + tr * 0.535f, cy - tr * 0.20f); path.lineTo(wx + tr * 0.545f, cy)
                path.lineTo(wx + tr * 0.535f, cy + tr * 0.20f); path.lineTo(wx + tr * 0.13f, cy + tr * 0.085f); path.close()
                p.color = Color.BLACK
                p.shader = LinearGradient(wx + tr * 0.13f, cy, wx + tr * 0.54f, cy, 0xFFC0C5CB.toInt(), 0xFF8E9399.toInt(), Shader.TileMode.CLAMP)
                if (fast) p.alpha = 90
                c.drawPath(path, p); p.shader = null; p.alpha = 255
                path.reset()
                path.moveTo(wx + tr * 0.22f, cy - tr * 0.045f); path.lineTo(wx + tr * 0.49f, cy - tr * 0.135f)
                path.lineTo(wx + tr * 0.49f, cy + tr * 0.135f); path.lineTo(wx + tr * 0.22f, cy + tr * 0.045f); path.close()
                p.color = 0xFF0B0B0D.toInt(); if (fast) p.alpha = 120
                c.drawPath(path, p); p.alpha = 255
                c.restore()
            }
            if (fast) { p.color = 0x6E3A3D42; c.drawCircle(wx, cy, tr * 0.53f, p) }
            p.color = 0xFF16171A.toInt(); c.drawCircle(wx, cy, tr * 0.17f, p)
            p.color = 0xFF7D8188.toInt()
            for (k in 0 until 5) { val a = wheelTurn + k * 1.2566f + 0.628f; c.drawCircle(wx + tr * 0.115f * cos(a), cy + tr * 0.115f * sin(a), tr * 0.022f, p) }
            p.color = 0xFF0A0A0B.toInt(); c.drawCircle(wx, cy, tr * 0.07f, p)
            p.style = Paint.Style.STROKE; p.strokeWidth = tr * 0.012f; p.color = 0xFF6E7278.toInt(); c.drawCircle(wx, cy, tr * 0.07f, p); p.style = Paint.Style.FILL
        }
        if (runProg >= 0f && !ghost) {
            // run: an arc round each tyre fills from the top with the progress; the amber marker blinks at 1 Hz
            p.style = Paint.Style.STROKE; p.strokeCap = Paint.Cap.ROUND; p.strokeWidth = tr * 0.09f
            for (wx in floatArrayOf(REAR_X, FRONT_X)) {
                r.set(wx - tr * 1.14f, cy - tr * 1.14f, wx + tr * 1.14f, cy + tr * 1.14f)
                p.color = 0x40FFFFFF; c.drawArc(r, 0f, 360f, false, p)
                p.color = runCol; c.drawArc(r, -90f, 360f * runProg.coerceIn(0.01f, 1f), false, p)
            }
            p.style = Paint.Style.FILL; p.strokeCap = Paint.Cap.BUTT
            if ((t % 1f) < 0.5f) {
                p.color = Color.BLACK
                p.shader = RadialGradient(J.MARKER_X, J.MARKER_Y, 16f, 0xCCFFB020.toInt(), Color.TRANSPARENT, Shader.TileMode.CLAMP)
                c.drawCircle(J.MARKER_X, J.MARKER_Y, 16f, p); p.shader = null
                p.color = 0xFFFFD27A.toInt(); r.set(J.MARKER_X - 5f, J.MARKER_Y - 2f, J.MARKER_X + 5f, J.MARKER_Y + 2f); c.drawRoundRect(r, 1.5f, 1.5f, p)
            }
        }
        if (ghost) c.restore()
        c.restore()
        if (lights) {   // small glows: they must not hide the nose or the spare
            val (hx, hy) = maskPt(ps, J.HEAD_X, J.HEAD_Y)
            p.color = Color.BLACK; p.shader = RadialGradient(hx, hy, len * 0.03f, 0xFFFFF6D8.toInt(), Color.TRANSPARENT, Shader.TileMode.CLAMP); c.drawCircle(hx, hy, len * 0.03f, p)
            val (tx, ty) = maskPt(ps, J.TAIL_X, J.TAIL_Y)
            p.color = Color.BLACK; p.shader = RadialGradient(tx, ty, len * 0.02f, 0xFFFF3B30.toInt(), Color.TRANSPARENT, Shader.TileMode.CLAMP); c.drawCircle(tx, ty, len * 0.02f, p)
            p.shader = null
        }
    }
    /** wheel turn angle for a ground speed (px per second) and the wheel radius on screen */
    /** capped at 7 rad/s so 25 fps never strobes the spokes backwards */
    private fun turn(speedPx: Float, z: Float) = td * minOf(speedPx / (TYRE_R * z), 7f)
    /** dust behind the rear wheel */
    private fun dust(c: Canvas, ps: Pose, col: Int) {
        val (x, y) = maskPt(ps, REAR_X - WHEEL_R * 0.5f, JeepShape.H - 2f); val len = JeepShape.W * ps.z
        for (i in 0 until 16) {
            val ph = ((t * 0.8f + i / 16f) % 1f)
            val dx = -ph * len * 0.55f; val dy = -ph * len * 0.14f + sin(i * 2.1f) * len * 0.02f
            val rr = len * (0.02f + 0.07f * ph)
            val a = ((1f - ph) * 0.45f * 255 * moving).toInt().coerceIn(0, 255)
            // a soft puff without a blur filter per particle (advisor: cheaper at 25 fps)
            p.color = Color.BLACK
            p.shader = RadialGradient(x + dx, y + dy, rr * 1.4f, (col and 0x00FFFFFF) or (a shl 24), col and 0x00FFFFFF, Shader.TileMode.CLAMP)
            c.drawCircle(x + dx, y + dy, rr * 1.4f, p)
        }
        p.shader = null
    }

    // ---------- scenes ----------
    private fun highway(c: Canvas, w: Float, h: Float) {   // dusk, the city slides by, lane dashes rush under the Jeep
        val speed = w * 0.6f; val off = td * speed
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
        val ps = at(0.18f, 0.66f, { road }, bounce = h * 0.003f * sin(t * 17f) * moving)
        jeep(c, ps, 0xFFE8E4F0.toInt(), 0xFF5D6378.toInt(), lights = true, wheelTurn = turn(speed, ps.z))
    }

    private fun rough(c: Canvas, w: Float, h: Float) {   // overcast, broken asphalt with bumps and potholes
        val speed = w * 0.2f; val off = td * speed
        sky(c, w, h, 0xFF2E3D4F.toInt(), 0xFFC7B9A6.toInt(), 0.75f)
        clouds(c, w, h, h * 0.18f, 0x55E6EEF5, 5, w * 0.16f, 0.01f)
        band(c, w, h, { x -> h * (0.55f - 0.08f * sin((x + off * 0.1f) / w * 5f)) }, 0xFF4F6372.toInt(), 0xFF35454F.toInt())
        haze(c, w, h * 0.45f, h * 0.66f, 0xFF9FB0BE.toInt())
        band(c, w, h, { x -> h * (0.66f - 0.05f * sin((x + off * 0.3f) / w * 7f + 1f)) }, 0xFF3F5A46.toInt(), 0xFF27322C.toInt())
        val period = w * 0.9f
        val g = { x: Float -> val u = x + off; h * 0.8f - h * 0.06f * gauss(rep(u, period, 0f), 0f, w * 0.05f) + h * 0.03f * gauss(rep(u, period, period * 0.5f), 0f, w * 0.03f) }
        band(c, w, h, g, 0xFF2B2F38.toInt(), 0xFF121419.toInt())
        p.color = 0xFF0E0F12.toInt(); p.style = Paint.Style.STROKE; p.strokeWidth = h * 0.008f
        every(w, w * 0.45f, off) { x, _ -> path.reset(); path.moveTo(x, h * 0.85f); path.lineTo(x + w * 0.03f, h * 0.89f); path.lineTo(x + w * 0.07f, h * 0.87f); path.lineTo(x + w * 0.1f, h * 0.92f); c.drawPath(path, p) }
        p.style = Paint.Style.FILL
        val ps = at(0.18f, 0.66f, g)
        jeep(c, ps, 0xFFEDF1F5.toInt(), 0xFF7D8792.toInt(), wheelTurn = turn(speed, ps.z))
    }

    private fun gravel(c: Canvas, w: Float, h: Float) {   // afternoon, pines, a dirt road, stones and dust
        val speed = w * 0.3f; val off = td * speed
        sky(c, w, h, 0xFF4F86BF.toInt(), 0xFFF1D7A8.toInt(), 0.8f)
        sun(c, w * 0.82f, h * 0.2f, h * 0.06f, 0xFFFFF1C4.toInt())
        clouds(c, w, h, h * 0.16f, 0x88FFFFFF.toInt(), 4, w * 0.13f, 0.012f)
        band(c, w, h, { x -> h * (0.6f - 0.07f * sin((x + off * 0.1f) / w * 4f + 0.5f)) }, 0xFF6E8A6A.toInt(), 0xFF4D664B.toInt())
        haze(c, w, h * 0.5f, h * 0.68f, 0xFFF1D7A8.toInt())
        every(w, w * 0.07f, off * 0.35f) { x, k -> fir(c, x, h * 0.72f, h * 0.3f, 0xFF2E4A33.toInt(), k) }
        val g = { x: Float -> h * 0.8f + h * 0.012f * sin((x + off) / w * 13f) }
        band(c, w, h, g, 0xFFB08A60.toInt(), 0xFF6E5236.toInt())
        val rnd = java.util.Random(7)
        repeat(70) {
            val fx = rnd.nextFloat(); val fy = rnd.nextFloat(); val fr = rnd.nextFloat()
            val sx = ((fx * w * 1.5f - off) % (w * 1.5f) + w * 1.5f) % (w * 1.5f) - w * 0.25f
            val sy = h * 0.83f + fy * h * 0.17f
            p.color = if (fr > 0.5f) 0xFFD9C4A3.toInt() else 0xFF8C7458.toInt(); c.drawCircle(sx, sy, h * (0.006f + fr * 0.012f), p)
        }
        val ps = at(0.18f, 0.66f, g)
        dust(c, ps, 0xFFD9C4A3.toInt())
        jeep(c, ps, 0xFFFFF8EE.toInt(), 0xFF9C8C78.toInt(), wheelTurn = turn(speed, ps.z))
    }

    private fun trail(c: Canvas, w: Float, h: Float) {   // forest at dusk, layers of firs, a wavy trail
        val speed = w * 0.18f; val off = td * speed
        sky(c, w, h, 0xFF1C3A2E.toInt(), 0xFFB9D49A.toInt(), 0.7f)
        every(w, w * 0.08f, off * 0.2f) { x, k -> fir(c, x, h * 0.66f, h * 0.34f, 0xFF3F6B4C.toInt(), k) }
        haze(c, w, h * 0.35f, h * 0.68f, 0xFFB9D49A.toInt())
        every(w, w * 0.11f, off * 0.5f) { x, k -> fir(c, x, h * 0.74f, h * 0.48f, 0xFF244634.toInt(), k + 1000) }
        val g = { x: Float -> h * 0.8f + h * 0.045f * sin((x + off) / w * 8f) + h * 0.015f * sin((x + off) / w * 23f) }
        band(c, w, h, g, 0xFF6B4E33.toInt(), 0xFF3A2A1C.toInt())
        p.color = 0x553A2A1C
        every(w, w * 0.12f, off) { x, _ -> c.drawRect(x, g(x) + h * 0.09f, x + w * 0.05f, g(x) + h * 0.105f, p) }
        val ps = at(0.18f, 0.66f, g)
        jeep(c, ps, 0xFFEFF5EA.toInt(), 0xFF5F7562.toInt(), lights = true, wheelTurn = turn(speed, ps.z))
    }

    private fun rock(c: Canvas, w: Float, h: Float) {   // alpine: the Jeep crawls slowly over boulders
        val speed = w * 0.07f; val off = td * speed
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
        val ps = at(0.17f, 0.58f, g)
        jeep(c, ps, 0xFFF2F7FF.toInt(), 0xFF6C7C92.toInt(), wheelTurn = turn(speed, ps.z))
    }

    /** a saguaro: trunk and two arms, rounded, lit from the left (owner: SAND needs a cactus) */
    private fun cactus(c: Canvas, x: Float, base: Float, hgt: Float, k: Int) {
        val tw = hgt * 0.16f
        val lit = 0xFF5E8F4E.toInt(); val dark = 0xFF33562E.toInt()
        fun stem(x0: Float, y0: Float, x1: Float, y1: Float, wd: Float) {
            r.set(minOf(x0, x1) - wd / 2, minOf(y0, y1) - wd / 2, maxOf(x0, x1) + wd / 2, maxOf(y0, y1) + wd / 2)
            p.color = Color.BLACK; p.shader = LinearGradient(r.left, 0f, r.right, 0f, lit, dark, Shader.TileMode.CLAMP)
            c.drawRoundRect(r, wd / 2, wd / 2, p); p.shader = null
        }
        val armL = if (k % 2 == 0) 0.45f else 0.55f; val armR = if (k % 2 == 0) 0.6f else 0.4f
        stem(x - tw * 1.6f, base - hgt * armL, x - tw * 1.6f, base - hgt * (armL + 0.28f), tw * 0.75f)   // left arm, up
        stem(x - tw * 1.6f, base - hgt * armL, x, base - hgt * armL, tw * 0.75f)
        stem(x + tw * 1.5f, base - hgt * armR, x + tw * 1.5f, base - hgt * (armR + 0.22f), tw * 0.7f)    // right arm, up
        stem(x, base - hgt * armR, x + tw * 1.5f, base - hgt * armR, tw * 0.7f)
        stem(x, base, x, base - hgt, tw)                                                                    // trunk
        p.color = 0x33000000; c.drawRect(x - tw * 0.05f, base - hgt + tw * 0.5f, x + tw * 0.05f, base, p)  // rib
    }
    private fun sand(c: Canvas, w: Float, h: Float) {   // desert noon: rolling over dunes, dust behind
        val speed = w * 0.22f; val off = td * speed
        sky(c, w, h, 0xFFF29A3F.toInt(), 0xFFFDE7B8.toInt(), 0.65f)
        sun(c, w * 0.8f, h * 0.25f, h * 0.11f, 0xFFFFF4D6.toInt())
        band(c, w, h, { x -> val u = x + off * 0.15f; h * (0.6f - 0.1f * gauss(rep(u, w * 1.1f, 0f), 0f, w * 0.2f) - 0.06f * gauss(rep(u, w * 0.7f, w * 0.3f), 0f, w * 0.12f)) }, 0xFFE9B36F.toInt(), 0xFFC98942.toInt())
        // saguaros on the far dunes, slower than the ground
        every(w, w * 0.42f, off * 0.45f) { x, k -> val gx = x + w * 0.1f * (((k * 7) % 5) - 2) / 2f; cactus(c, gx, h * (0.6f + 0.03f * sin(gx / w * 5f)), h * (0.24f + 0.06f * (((k % 3) + 3) % 3)), k) }
        val g = { x: Float -> val u = x + off; h * 0.84f - h * 0.12f * (0.5f + 0.5f * sin(u / w * 3.4f)) - h * 0.03f * sin(u / w * 9f) }
        band(c, w, h, g, 0xFFF0C27E.toInt(), 0xFFB97734.toInt())
        p.color = 0x22000000; p.style = Paint.Style.STROKE; p.strokeWidth = h * 0.006f
        for (i in 0 until 4) { path.reset(); var x = 0f; path.moveTo(0f, g(0f) + h * (0.04f + i * 0.03f)); while (x < w) { path.lineTo(x, g(x) + h * (0.04f + i * 0.03f) + h * 0.006f * sin((x + off) / w * 40)); x += w / 40 }; c.drawPath(path, p) }
        p.style = Paint.Style.FILL
        val ps = at(0.18f, 0.66f, g)
        dust(c, ps, 0xFFF3CD8F.toInt())
        jeep(c, ps, 0xFFFFFFFF.toInt(), 0xFF8E6440.toInt(), wheelTurn = turn(speed, ps.z))
    }

    private fun recovery(c: Canvas, w: Float, h: Float) {   // stuck: the wheels spin in the sand and throw it back
        sky(c, w, h, 0xFF7A2E3B.toInt(), 0xFFF5A35C.toInt(), 0.7f)
        sun(c, w * 0.2f, h * 0.62f, h * 0.09f, 0xFFFFC27A.toInt())
        band(c, w, h, { x -> h * (0.66f - 0.06f * sin(x / w * 4f)) }, 0xFFD18A4C.toInt(), 0xFFA8652F.toInt())
        val shake = h * 0.004f * sin(t * 23f) * moving   // the body shakes while the wheels dig
        val ps = at(0.20f, 0.64f, { h * 0.8f }, bounce = shake)
        val sunk = h * 0.06f
        jeep(c, ps, 0xFFFFFFFF.toInt(), 0xFF7E4A38.toInt(), lights = true, wheelTurn = td * 14f, sunk = sunk, spin = 14f * moving)
        // the sand the wheels sit in, piled a little higher at each wheel
        val (rx, _) = maskPt(ps, REAR_X, WHEEL_Y); val (fx, _) = maskPt(ps, FRONT_X, WHEEL_Y)
        band(c, w, h, { x -> h * (0.84f - 0.015f * sin(x / w * 11f)) - h * 0.03f * (gauss(x, rx, w * 0.07f) + gauss(x, fx, w * 0.07f)) }, 0xFFE3A15B.toInt(), 0xFFB06A2E.toInt())
        // sand thrown back and up from both wheels, falling again
        for ((wx, seed) in listOf(rx to 0.3f, fx to 0.7f)) {
            for (i in 0 until 28) {
                val ph = ((t * 2.2f + i / 28f + seed) % 1f)
                val vx = -(0.3f + 0.3f * ((i * 37) % 10) / 10f) * w; val vy = -(0.5f + 0.35f * ((i * 53) % 10) / 10f) * h
                val x = wx - w * 0.02f + vx * ph * 0.5f; val y = h * 0.82f + vy * ph * 0.5f + 0.9f * h * ph * ph
                val a = ((1f - ph) * 230 * moving).toInt().coerceIn(0, 255)
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
        p.color = Color.BLACK; p.shader = RadialGradient(w * 0.38f, h * 0.3f, w * 0.55f, 0x17FFFFFF, Color.TRANSPARENT, Shader.TileMode.CLAMP)   // a neutral softbox (advisor)
        c.drawRect(0f, 0f, w, h, p); p.shader = null
        p.color = Color.BLACK; p.shader = LinearGradient(0f, h * 0.8f, 0f, h, 0xFF1F2024.toInt(), 0xFF0C0C0E.toInt(), Shader.TileMode.CLAMP); c.drawRect(0f, h * 0.8f, w, h, p); p.shader = null
        p.color = 0x0FFFFFFF; c.drawRect(0f, h * 0.8f, w, h * 0.8f + 1f, p)
        val ps = at(0.18f, 0.66f, { h * 0.8f })
        c.save(); c.scale(1f, -1f, 0f, h * 0.8f); jeep(c, ps, 0xFF3A3D45.toInt(), 0xFF1A1B20.toInt(), wheelTurn = 0f); c.restore()
        p.color = Color.BLACK; p.shader = LinearGradient(0f, h * 0.8f, 0f, h, 0x991A1B20.toInt(), 0xFF1A1B20.toInt(), Shader.TileMode.CLAMP); c.drawRect(0f, h * 0.8f, w, h, p); p.shader = null
        jeep(c, ps, 0xFFF2F2F7.toInt(), 0xFF6E717C.toInt(), wheelTurn = 0f)
        val gx = w * 0.86f; val gy = h * 0.3f; val gr = h * 0.15f
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
