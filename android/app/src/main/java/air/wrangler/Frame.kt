package air.wrangler

/** One DinMeter screen as a display list (firmware dlist.h). */
class Frame(val w: Int, val h: Int, val bg: Int, val items: List<Item>) {
    class Item(val kind: Int, val x: Int, val y: Int, val w: Int, val h: Int, val color: Int, val aux: Int, val datum: Int, val text: String)

    companion object {
        const val TEXT = 1; const val FILL_RECT = 2; const val FILL_RRECT = 3; const val DRAW_RECT = 4
        const val FILL_CIRCLE = 5; const val ARROW = 6; const val ICON = 7

        /** RGB565 -> ARGB */
        fun argb(c: Int): Int {
            val r = (c shr 11) and 0x1F; val g = (c shr 5) and 0x3F; val b = c and 0x1F
            return (0xFF shl 24) or ((r * 255 / 31) shl 16) or ((g * 255 / 63) shl 8) or (b * 255 / 31)
        }

        fun decode(b: ByteArray): Frame? {
            if (b.size < 8 || b[0] != 'D'.code.toByte() || b[1].toInt() != 1) return null
            fun u8(i: Int) = b[i].toInt() and 0xFF
            fun s16(i: Int) = ((b[i].toInt() and 0xFF) or (b[i + 1].toInt() shl 8)).toShort().toInt()
            fun u16(i: Int) = (b[i].toInt() and 0xFF) or ((b[i + 1].toInt() and 0xFF) shl 8)
            val w = u8(3); val h = u8(4); val bg = argb(u16(5)); val n = u8(7)
            var p = 8
            val items = ArrayList<Item>(n)
            repeat(n) {
                if (p + 11 > b.size) return@repeat
                val kind = u8(p)
                var text = ""
                val it = Item(kind, s16(p + 1), s16(p + 3), u8(p + 5), u8(p + 6), argb(u16(p + 7)), u8(p + 9), u8(p + 10), "")
                p += 11
                if (kind == TEXT) {
                    val len = u8(p); p += 1
                    if (p + len > b.size) return@repeat
                    text = String(b, p, len, Charsets.ISO_8859_1); p += len
                }
                items.add(Item(it.kind, it.x, it.y, it.w, it.h, it.color, it.aux, it.datum, text))
            }
            return Frame(w, h, bg, items)
        }
    }
}
