package air.wrangler

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import kotlin.math.PI
import kotlin.math.min
import kotlin.math.sin

/**
 * The DinMeter's sounds, repeated on the phone (owner). The firmware publishes the id of every sound it starts
 * (sound.h mark(), STATE bytes 40-41); the same tones are synthesised here: frequency Hz, length ms, gap ms.
 * Played on the media stream, so the phone's media volume sets the level.
 */
object Sounds {
    private class N(val f: Int, val ms: Int, val gap: Int)
    private const val SR = 44100
    private val defs: Map<Int, List<N>> = mapOf(
        1 to listOf(N(2600, 15, 0)),                                   // knob tick
        2 to listOf(N(3500, 45, 0)),                                   // press
        3 to listOf(N(2200, 160, 0)),                                  // hold
        4 to listOf(N(2400, 70, 25), N(3000, 70, 25), N(3700, 130, 0)),   // menu opens
        5 to List(1) { N(3500, 120, 60) },                             // beep
        6 to listOf(N(3500, 1500, 0)),                                 // target reached
        7 to List(2) { N(3500, 120, 60) },                             // already within the band
        8 to List(3) { N(1800, 120, 100) },                            // off target
        9 to List(3) { N(1800, 120, 100) },                            // error
        10 to List(10) { N(1800, 90, 110) },                           // over pressure / air with no signal
        11 to List(2) { N(1800, 110, 90) },                            // start refused
        12 to listOf(N(2800, 15, 0)),                                  // compressor tick
        13 to listOf(N(1000, 60, 0)),                                  // valve knock
        14 to List(3) { N(3500, 120, 60) },
    )
    private val tracks = HashMap<Int, AudioTrack>()

    private fun build(id: Int): AudioTrack? {
        val notes = defs[id] ?: return null
        val total = notes.sumOf { (it.ms + it.gap) * SR / 1000 }
        val pcm = ShortArray(total + SR / 100)
        var p = 0
        for (n in notes) {
            val len = n.ms * SR / 1000; val ramp = min(len / 4, SR / 500)   // 2 ms in and out, no clicks
            for (i in 0 until len) {
                val t = i.toDouble() / SR
                // a piezo buzzer is not a pure sine: a little third harmonic
                var v = 0.75 * sin(2 * PI * n.f * t) + 0.25 * sin(2 * PI * 3 * n.f * t)
                val env = when { i < ramp -> i.toDouble() / ramp; i > len - ramp -> (len - i).toDouble() / ramp; else -> 1.0 }
                v *= env * 0.6
                pcm[p + i] = (v * Short.MAX_VALUE).toInt().toShort()
            }
            p += len + n.gap * SR / 1000
        }
        return try {
            val t = AudioTrack.Builder()
                .setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build())
                .setAudioFormat(AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(SR).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build())
                .setTransferMode(AudioTrack.MODE_STATIC).setBufferSizeInBytes(pcm.size * 2).build()
            t.write(pcm, 0, pcm.size); t
        } catch (e: Exception) { null }
    }

    fun play(id: Int) {
        val t = tracks.getOrPut(id) { build(id) ?: return }
        try { if (t.playState == AudioTrack.PLAYSTATE_PLAYING) t.stop(); t.reloadStaticData(); t.play() } catch (_: Exception) {}
    }

    fun release() { tracks.values.forEach { try { it.release() } catch (_: Exception) {} }; tracks.clear() }
}
