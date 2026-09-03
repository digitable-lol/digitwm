/*
 * Сгенерировано flang (бэкенд C, flang/self/emit-c.flang). Не редактировать руками.
 * Модуль flang: «Placement».
 * Файл: объявления: конструкторы значений и функции программы.
 * Правьте исходник на flang и печатайте заново: любая правка здесь потеряется.
 */
#ifndef PLACEMENT_H
#define PLACEMENT_H

#include "flang_runtime.h"

/*
 * Контракт вызова: функция кладёт результат в *result и возвращает FL_OK
 * либо НЕ трогает *result и возвращает FL_ERROR, заполнив *error (его можно
 * передать NULL). Результат живёт в арене контекста — до ближайшего
 * fl_arena_reset; чтобы сохранить его надолго, скопируйте в свою память.
 *
 *   fl_arena arena;
 *   fl_ctx ctx;
 *   fl_error error;
 *   fl_value result;
 *   fl_arena_init(&arena);
 *   fl_ctx_init(&ctx, &arena);
 *   if (…(&ctx, …, &result, &error) != FL_OK) { … error.code, error.message … }
 *   fl_arena_release(&arena);
 */

/*
 * Функция flang «Место: своя колонка».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_mesto_svoya_kolonka(fl_ctx *ctx, fl_value *result, fl_error *error);

/*
 * Функция flang «Место: в стопку».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_mesto_v_stopku(fl_ctx *ctx, fl_value *result, fl_error *error);

/*
 * Функция flang «Место: плавающее».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_mesto_plavayuschee(fl_ctx *ctx, fl_value *result, fl_error *error);

/*
 * Функция flang «Место: во весь экран».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_mesto_vo_ves_ekran(fl_ctx *ctx, fl_value *result, fl_error *error);

/*
 * Функция flang «Правило: без правила».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_pravilo_bez_pravila(fl_ctx *ctx, fl_value *result, fl_error *error);

/*
 * Функция flang «Правило: в стопку».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_pravilo_v_stopku(fl_ctx *ctx, fl_value *result, fl_error *error);

/*
 * Функция flang «Правило: плавающее».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_pravilo_plavayuschee(fl_ctx *ctx, fl_value *result, fl_error *error);

/*
 * Функция flang «Куда положить окно».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param fokus — «фокус»: число
 * @param dochernee — «дочернее»: число
 * @param dialog — «диалог»: число
 * @param dok — «док»: число
 * @param voves — «вовесь»: число
 * @param ukazanie — «указание»: число
 * @return значение: число
 */
fl_status placement_kuda_polozhit_okno(fl_ctx *ctx, fl_value fokus, fl_value dochernee, fl_value dialog, fl_value dok, fl_value voves, fl_value ukazanie, fl_value *result, fl_error *error);

/*
 * Функция flang «Фокус после закрытия».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param nomer — «номер»: число
 * @param kolonok — «колонок»: число
 * @param poslednyaya — «последняя»: число
 * @param odno — «одно»: число
 * @return значение: число
 */
fl_status placement_fokus_posle_zakrytiya(fl_ctx *ctx, fl_value nomer, fl_value kolonok, fl_value poslednyaya, fl_value odno, fl_value *result, fl_error *error);

/*
 * Вызов функции по её исходному имени flang. Нужен прогонщику и всякому,
 * кто связывает программу с внешним миром динамически (скрипт, FFI, тест).
 */
fl_status placement_call(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error);

/*
 * ТО ЖЕ, НО ЧЕРЕЗ ГРАНИЦУ ВХОДА: объявленные типы сверяются ДО вызова.
 * Значения, пришедшие снаружи — из JSON, из другого языка, от человека, —
 * обязаны заходить ЗДЕСЬ: `_call` их не сверяет, а доказательство завершения
 * `тотальной` стоит НА ТИПЕ и вместе с типом теряется.
 */
fl_status placement_enter(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error);

/*
 * Объявленные типы параметров — данными. Прогонщик сверяет по ним значения,
 * пришедшие снаружи, ДО вызова: доказательство завершения `тотальной` стоит
 * на типе, и значение вне типа выносит вместе с типом и доказательство.
 */
const fl_entry_table *placement_entry(void);

#endif /* PLACEMENT_H */
