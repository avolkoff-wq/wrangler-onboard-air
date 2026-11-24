package air.wrangler

/** Jeep Wrangler JL Unlimited Rubicon, side view, traced from the owner's reference photo (tools/jl_blueprint.py,
 *  generated). Mask units 512 x 226, nose right, ground at y = 226; pivot = ground under the rear wheel.
 *  CONTOURS: the outer body, the three windows, the door seams. Below: where SceneArt draws each part. */
object JeepShape {
    const val W = 512f; const val H = 226f
    const val PIVOT_X = 102.9f; const val PIVOT_Y = 226f
    const val REAR_X = 102.9f; const val FRONT_X = 417.45f
    const val WHEEL_R = 42.39f; const val WHEEL_Y = 183.61f
    const val ROOF_Y = 15.4f; const val BELT_Y = 87.76f; const val BODY_BOTTOM = 166.6f
    const val ROOF_FRONT_X = 281.1f
    val CONTOURS = listOf(
        floatArrayOf(41.72f, 143.27f, 41.23f, 117.03f, 41.07f, 103.10f, 41.07f, 87.54f, 41.23f, 75.56f, 41.72f, 55.14f, 42.10f, 45.42f, 42.74f, 36.68f, 43.45f, 31.82f, 44.90f, 25.98f, 45.93f, 23.20f, 47.55f, 20.67f, 49.22f, 19.24f, 52.63f, 17.50f, 63.16f, 16.70f, 91.34f, 16.05f, 127.79f, 15.77f, 208.96f, 15.57f, 245.24f, 15.70f, 272.95f, 16.29f, 282.83f, 17.13f, 284.77f, 19.07f, 286.86f, 22.23f, 291.17f, 29.74f, 296.26f, 39.12f, 307.25f, 59.72f, 312.74f, 68.97f, 318.25f, 76.10f, 321.93f, 78.73f, 327.48f, 79.51f, 332.91f, 79.98f, 343.66f, 80.63f, 356.16f, 81.24f, 382.89f, 82.40f, 396.50f, 83.09f, 410.59f, 84.00f, 420.47f, 84.82f, 436.03f, 86.38f, 443.80f, 87.33f, 451.58f, 88.62f, 456.76f, 89.83f, 464.54f, 92.17f, 468.23f, 93.56f, 471.54f, 95.41f, 473.35f, 97.09f, 475.49f, 100.30f, 476.55f, 102.75f, 477.58f, 106.90f, 478.25f, 111.36f, 479.23f, 120.59f, 479.58f, 125.34f, 479.68f, 130.37f, 479.49f, 133.98f, 478.80f, 139.82f, 478.34f, 143.27f, 477.63f, 147.81f, 476.90f, 151.91f, 475.45f, 159.69f, 457.52f, 163.14f, 405.19f, 165.74f, 335.91f, 166.60f, 180.39f, 166.60f, 111.22f, 163.68f, 59.21f, 154.94f),
        floatArrayOf(74.12f, 34.84f, 97.93f, 34.84f, 108.52f, 36.71f, 116.45f, 42.31f, 119.10f, 49.79f, 119.10f, 66.60f, 116.45f, 74.08f, 108.52f, 79.68f, 97.93f, 81.55f, 74.12f, 81.55f, 63.53f, 79.68f, 55.60f, 74.08f, 52.95f, 66.60f, 52.95f, 49.79f, 55.60f, 42.31f, 63.53f, 36.71f),
        floatArrayOf(155.28f, 36.19f, 177.15f, 36.19f, 186.87f, 38.11f, 194.16f, 43.88f, 196.59f, 51.57f, 196.59f, 68.87f, 194.16f, 76.56f, 186.87f, 82.33f, 177.15f, 84.25f, 155.28f, 84.25f, 145.56f, 82.33f, 138.27f, 76.56f, 135.84f, 68.87f, 135.84f, 51.57f, 138.27f, 43.88f, 145.56f, 38.11f),
        floatArrayOf(235.47f, 39.16f, 254.91f, 39.16f, 264.23f, 40.26f, 272.75f, 43.57f, 277.63f, 47.97f, 283.76f, 57.89f, 286.53f, 63.10f, 288.74f, 68.84f, 289.63f, 73.18f, 290.12f, 80.47f, 287.44f, 83.71f, 278.92f, 86.14f, 267.39f, 86.95f, 241.35f, 86.95f, 229.77f, 85.04f, 221.08f, 79.30f, 218.19f, 71.66f, 218.19f, 54.45f, 220.35f, 46.81f, 226.83f, 41.07f),
        floatArrayOf(128.01f, 32.95f, 129.63f, 32.95f, 129.63f, 157.15f, 128.01f, 157.15f),
        floatArrayOf(206.31f, 32.95f, 207.93f, 32.95f, 207.93f, 159.85f, 206.31f, 159.85f),
        floatArrayOf(310.80f, 80.20f, 312.42f, 80.20f, 313.23f, 161.20f, 311.61f, 161.20f),
        floatArrayOf(129.63f, 155.80f, 312.15f, 155.80f, 312.15f, 157.42f, 129.63f, 157.42f),
    )
    val FLARE_REAR = floatArrayOf(41.61f, 155.80f, 62.40f, 107.74f, 137.19f, 106.66f, 160.95f, 159.04f)
    val FLARE_FRONT = floatArrayOf(332.40f, 172.00f, 363.45f, 112.60f, 471.45f, 114.22f, 486.30f, 138.25f)
    val ROCK_RAIL = floatArrayOf(155.55f, 160.66f, 336.45f, 172.54f)
    val BUMPER_FRONT = floatArrayOf(474.15f, 126.1f, 510.6f, 172.0f)
    val BUMPER_REAR = floatArrayOf(29.19f, 133.66f, 52.95f, 153.1f)
    val SPARE = floatArrayOf(4.35f, 58.6f, 42.15f, 155.8f)
    val TAIL_LAMP = floatArrayOf(31.35f, 94.24f, 43.23f, 115.84f)
    val MIRROR = floatArrayOf(276.51f, 66.16f, 299.19f, 88.84f)
    val HINGES = listOf(floatArrayOf(190.11f, 101.8f, 204.15f, 112.06f), floatArrayOf(190.11f, 136.36f, 204.15f, 146.08f), floatArrayOf(293.79f, 105.04f, 309.99f, 115.3f), floatArrayOf(293.79f, 139.6f, 309.99f, 149.32f))
    val HANDLES = listOf(floatArrayOf(138.0f, 100.18f, 161.49f, 105.58f), floatArrayOf(213.6f, 102.88f, 238.17f, 108.28f))
    val VENT = floatArrayOf(349.41f, 115.3f, 366.15f, 130.42f)
    val ANTENNA = floatArrayOf(319.71f, 8.65f, 321.87f, 112.6f)
    val BADGE = floatArrayOf(325.65f, 146.35f)
    val DECAL = floatArrayOf(354.0f, 105.58f, 446.61f)
    val MARKER = floatArrayOf(457.95f, 128.8f, 471.45f, 134.2f)
    val HEADLAMP = floatArrayOf(477.39f, 109.9f)
}
