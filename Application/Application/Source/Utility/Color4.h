/**
 * @file Color4.h
 * @brief RGBAÌF
 */

#pragma once

#include <array>

namespace Framework::Utility {
    /**
     * @class Color4
     * @brief F
     */
    class Color4 {
    public:
        float r; //!< r¬ª
        float g; //!< g¬ª
        float b; //!< b¬ª
        float a; //!< a¬ª
    public:
        static const Color4 BLACK; //!< 
        static const Color4 WHITE; //!< 
    public:
        /**
         * @brief RXgN^
         */
        Color4();
        /**
         * @brief RXgN^
         * @param r r¬ª(0.0`1.0)
         * @param g g¬ª(0.0`1.0)
         * @param b b¬ª(0.0`1.0)
         * @param a a¬ª(0.0`1.0)
         */
        Color4(float r, float g, float b, float a);
        /**
         * @brief RXgN^
         * @param color Fzñ
         */
        Color4(float color[4]);
        /**
         * @brief Rs[RXgN^
         */
        Color4(const Color4& c);
        /**
         * @brief fXgN^
         */
        ~Color4();
        /**
         * @brief ãüZq
         */
        Color4& operator=(const Color4& c) &;
        /**
         * @brief ãüZq
         */
        Color4& operator=(Color4&& c) & noexcept;
        /**
         * vPvXZq
         */
        Color4 operator+();
        /**
         * @brief P}CiXZq
         */
        Color4 operator-();
        /**
         * @brief ÁZãüZq
         */
        Color4& operator+=(const Color4& c);
        /**
         * @brief ¸ZãüZq
         */
        Color4& operator-=(const Color4& c);
        /**
         * @brief æZãüZq
         */
        Color4& operator*=(const Color4& c);
        /**
         * @brief æZãüZq
         */
        Color4& operator*=(float s);
        /**
         * @brief ZãüZq
         */
        Color4& operator/=(float s);

        /**
         * @brief zñÌæ¾
         * @param c ßèl
         */
        void get(float c[4]) const;
        /**
         * @brief FðzñÅæ¾
         * @return r,g,b,aÌÉi[³ê½zñ
         */
        std::array<float, 4> get() const;
        /**
         * @brief 0`1ÌÔÉNv·é
         */
        Color4& saturate();
        /**
         * @brief âÔ
         * @param a F1
         * @param b F2
         * @param t âÔW
         */
        static Color4 lerp(const Color4& a, const Color4& b, float t);
        /**
         * @brief O[XP[ÉÏ··é
         */
        static Color4 grayScale(const Color4& c);
    };
    /**
     * @brief ¿är
     */
    bool operator==(const Color4& c1, const Color4& c2);
    /**
     * @brief ¿är
     */
    bool operator!=(const Color4& c1, const Color4& c2);

    /**
     * @brief ÁZ
     */
    Color4 operator+(const Color4& c1, const Color4& c2);
    /**
     * @brief ¸Z
     */
    Color4 operator-(const Color4& c1, const Color4& c2);
    /**
     * @brief æZ
     */
    Color4 operator*(const Color4& c1, const Color4& c2);
    /**
     * @brief æZ
     */
    Color4 operator*(const Color4& c, float s);
    /**
     * @brief æZ
     */
    Color4 operator*(float s, const Color4& c);
    /**
     * @brief Z
     */
    Color4 operator/(const Color4& c, float s);
} // namespace Framework::Utility
