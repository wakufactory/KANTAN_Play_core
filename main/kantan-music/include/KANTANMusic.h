// SPDX-License-Identifier: LicenseRef-KANTAN-Music
// This file is licensed under the KANTAN Music API License.
// See main/kantan-music/LICENSE_KANTAN_MUSIC.md for details.

#ifndef KANTAN_MUSIC_H
#define KANTAN_MUSIC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief コード構成音をどのように作るか（ボイシング）の種類
 *
 */
typedef enum {
    KANTANMusic_Voicing_Close = 0, /**< クローズドボイシング（ピアノなど） */
    KANTANMusic_Voicing_Guitar,    /**< ギター */
    KANTANMusic_Voicing_Static,    /**< Staticボイシング（ベースライン向け） */
    KANTANMusic_Voicing_Ukulele,   /**< ウクレレ */

    /** 以下はメロディボイシング。選択したスケールに応じて、メロディ演奏に適したノート番号を返します。
     * メロディボイシングでは、KANTANMusic_GetMidiNoteNumber 関数の引数のうち、以下のものだけ使用します。他の引数は無視されます。
     * - pitch: メロディの構成音のうち、低い方から何番目の音を得るか。1 以上 6 以下の整数。
     * - key: C/Am から数えて何番目のキーか。0 以上 11 以下の整数。
     * - position: メロディのポジションを1/12オクターブ単位で指定する。-4: 4/12(=1/3)オクターブ下のポジションのメロディ、0: そのまま、1: 1/12オクターブ上のポジションのメロディ。-36 以上 36 以下の整数。
     */
    KANTANMusic_Voicing_Melody_Major,       /**< メジャースケールのメロディボイシング */
    KANTANMusic_Voicing_Melody_MajorPentatonic, /**< メジャーペンタトニックスケールのメロディボイシング */
    KANTANMusic_Voicing_Melody_Chromatic,       /**< クロマティックスケールのメロディボイシング */
    KANTANMusic_MAX_VOICING,
} KANTANMusic_Voicing;

/**
 * @brief コードに付加するモディファイアーの種類
 *
 */
typedef enum {
    KANTANMusic_Modifier_None = 0,
    KANTANMusic_Modifier_dim,
    KANTANMusic_Modifier_m7_5,
    KANTANMusic_Modifier_sus4,
    KANTANMusic_Modifier_6,
    KANTANMusic_Modifier_7,
    KANTANMusic_Modifier_RESERVED, /**< この値は利用できません。 */
    KANTANMusic_Modifier_Add9,
    KANTANMusic_Modifier_M7,
    KANTANMusic_Modifier_aug,
    KANTANMusic_Modifier_7sus4,
    KANTANMusic_Modifier_dim7,
    KANTANMusic_MAX_MODIFIER,
} KANTANMusic_Modifier;

/**
 * @brief KANTANMusic_GetMidiNoteNumber関数に指定するオプションをまとめた構造体
 */
typedef struct
{
    KANTANMusic_Voicing voicing;   /**< ボイシング方法をKANTANMusic_Voicing列挙体から選ぶ。既定値は KANTANMusic_Voicing_Close。 */
    KANTANMusic_Modifier modifier; /**< コードのモディファイアー（dimなど）をKANTANMusic_Modifier列挙体から選ぶ。既定値は KANTANMusic_Modifier_None。 */
    int semitone_shift;            /**< 指定したコード番号(degree)から半音ずらしたコードの構成音を得る。-1: 半音下のコード、0: そのまま、1: 半音上のコード。既定値は 0。 */
    int bass_degree;               /**< オンコードを演奏する際の、ベース音のコード番号。1 以上 7 以下の整数。0 を指定すると、通常のコードを演奏する。既定値は 0。 */
    int bass_semitone_shift;       /**< オンコードを演奏する際に、ベース音を半音ずらす。-1: ベース音を半音下のコード、0: そのまま、1: ベース音を半音上のコード。既定値は 0。 */
    int position;                  /**< ポジションを1/12オクターブ単位で指定する。 -4: 4/12(=1/3)オクターブ下のポジションのコード、0: そのまま、1: 1/12オクターブ上のポジションのコード。既定値は 0。
                                        4/12(=1/3)上のポジションを選択したとき、キーを４つ下げて、最終的に出力されるノート番号を４つ上げたものと同じになる */
    bool minor_swap;               /**< trueの場合、メジャーコードをマイナーコードにし、マイナーコードをメジャーコードにする。falseの場合はそのまま。既定値は false。 */
} KANTANMusic_GetMidiNoteNumberOptions;

/**
 * @brief KANTANMusic_GetMidiNoteNumberOptions構造体をデフォルト値で初期化するマクロ
 * @param options_pointer デフォルト値で初期化するKANTANMusic_GetMidiNoteNumberOptionsへのポインター
 * @details 各オプションを以下の値で初期化する。
 * - voicing: KANTANMusic_Voicing_Close
 * - minor_swap: false
 * - modifier: KANTANMusic_Modifier_None
 * - semitone_shift: 0
 * - bass_degree: 0
 * - bass_semitone_shift: 0
 * - position: 0
 * - minor_swap: false
 */
#define KANTANMusic_GetMidiNoteNumber_SetDefaultOptions(options_pointer) \
    {                                                                    \
        (options_pointer)->voicing = KANTANMusic_Voicing_Close;          \
        (options_pointer)->modifier = KANTANMusic_Modifier_None;         \
        (options_pointer)->semitone_shift = 0;                           \
        (options_pointer)->bass_degree = 0;                              \
        (options_pointer)->bass_semitone_shift = 0;                      \
        (options_pointer)->position = 0;                                 \
        (options_pointer)->minor_swap = false;                           \
    }

/**
 * @brief コードを構成する音のMIDIノート番号を、カンタンに導き出す関数
 *
 * @param pitch 構成音のうち、低い方から何番目の音を得るか。1 以上 6 以下の整数。
 * @param degree 数値化したコードの番号。1 以上 7 以下の整数。
 * @param key C/Am から数えて何番目のキーか。0 以上 11 以下の整数。
 * -  0: C/Am
 * -  1: Db/Bbm
 * -  2: D/Bm
 * -  3: Eb/Cm
 * -  4: E/C#m
 * -  5: F/Dm
 * -  6: F#/D#m
 * -  7: G/Em
 * -  8: Ab/Fm
 * -  9: A/F#m
 * - 10: Bb/Gm
 * - 11: B/G#m
 *
 * @param options 任意指定のオプション。デフォルト値で関数を呼び出す場合は、NULLを指定する。
 * オプションを変更する場合、KANTANMusic_GetMidiNoteNumberOptions構造体を、
 * KANTANMusic_GetMidiNoteNumber_SetDefaultOptionsマクロでデフォルト値に初期化してから、
 * 各オプションを変更して引数に指定する。
 * @return uint8_t MIDIノート番号。0 の場合は、ミュート音として扱い、発音しないようにする。
 */
uint8_t KANTANMusic_GetMidiNoteNumber(
    int pitch, int degree, int key,
    const KANTANMusic_GetMidiNoteNumberOptions* options);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KANTAN_MUSIC_H
