/*
 * Сгенерировано flang (бэкенд C, flang/self/emit-c.flang). Не редактировать руками.
 * Модуль flang: «Viewport».
 * Файл: реализация.
 * Правьте исходник на flang и печатайте заново: любая правка здесь потеряется.
 */
#include "viewport.h"

#include <string.h>


/*
 * Функция flang «Смещение после смены окна».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param smeschenie — «смещение»: число
 * @param holst — «холст»: число
 * @return значение: число
 */
fl_status viewport_smeschenie_posle_smeny_okna(fl_ctx *ctx, fl_value okno, fl_value smeschenie, fl_value holst, fl_value *result, fl_error *error) {
  if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
  bool fl_t1 = false;
  FL_TRY(fl_cond(ctx, fl_flag((holst.as.number - okno.as.number) < 0.0), &fl_t1, error));
  fl_value fl_t2 = fl_nothing();
  if (fl_t1) {
    fl_t2 = fl_number(0.0);
  } else {
    if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
    fl_t2 = fl_number(holst.as.number - okno.as.number);
  }
  const fl_value predel = fl_t2; /* пусть «предел» */
  if (smeschenie.tag != FL_NUMBER || predel.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, smeschenie, predel, error));
  bool fl_t3 = false;
  FL_TRY(fl_cond(ctx, fl_flag(smeschenie.as.number > predel.as.number), &fl_t3, error));
  fl_value fl_t4 = fl_nothing();
  if (fl_t3) {
    fl_t4 = predel;
  } else {
    fl_t4 = smeschenie;
  }
  const fl_value sverhu = fl_t4; /* пусть «сверху» */
  if (sverhu.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, sverhu, fl_number(0.0), error));
  bool fl_t5 = false;
  FL_TRY(fl_cond(ctx, fl_flag(sverhu.as.number < 0.0), &fl_t5, error));
  fl_value fl_t6 = fl_nothing();
  if (fl_t5) {
    fl_t6 = fl_number(0.0);
  } else {
    fl_t6 = sverhu;
  }
  const fl_value fl_t7 = fl_t6;
  if (fl_t7.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t7, fl_number(0.0), error));
  /* постусловие «смещение не уходит влево от начала холста» */
  bool fl_t8 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t7.as.number >= 0.0), "смещение не уходит влево от начала холста", "Смещение после смены окна", &fl_t8, error));
  if (!fl_t8) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «смещение не уходит влево от начала холста» функции «Смещение после смены окна»");
  }
  if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
  bool fl_t9 = false;
  FL_TRY(fl_cond(ctx, fl_flag((holst.as.number - okno.as.number) < 0.0), &fl_t9, error));
  fl_value fl_t10 = fl_nothing();
  if (fl_t9) {
    fl_t10 = fl_number(0.0);
  } else {
    if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
    fl_t10 = fl_number(holst.as.number - okno.as.number);
  }
  if (fl_t7.tag != FL_NUMBER || fl_t10.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t7, fl_t10, error));
  /* постусловие «смещение не уходит за конец холста» */
  bool fl_t11 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t7.as.number <= fl_t10.as.number), "смещение не уходит за конец холста", "Смещение после смены окна", &fl_t11, error));
  if (!fl_t11) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «смещение не уходит за конец холста» функции «Смещение после смены окна»");
  }
  if (smeschenie.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, smeschenie, fl_number(0.0), error));
  bool fl_t12 = false;
  FL_TRY(fl_cond(ctx, fl_flag(smeschenie.as.number >= 0.0), &fl_t12, error));
  fl_value fl_t13 = fl_nothing();
  if (fl_t12) {
    if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
    bool fl_t14 = false;
    FL_TRY(fl_cond(ctx, fl_flag((holst.as.number - okno.as.number) < 0.0), &fl_t14, error));
    fl_value fl_t15 = fl_nothing();
    if (fl_t14) {
      fl_t15 = fl_number(0.0);
    } else {
      if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
      fl_t15 = fl_number(holst.as.number - okno.as.number);
    }
    if (smeschenie.tag != FL_NUMBER || fl_t15.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, smeschenie, fl_t15, error));
    fl_t13 = fl_flag(smeschenie.as.number <= fl_t15.as.number);
  } else {
    fl_t13 = fl_flag(false);
  }
  bool fl_t16 = false;
  FL_TRY(fl_cond(ctx, fl_t13, &fl_t16, error));
  fl_value fl_t17 = fl_nothing();
  if (fl_t16) {
    fl_t17 = fl_flag(fl_equal(fl_t7, smeschenie));
  } else {
    fl_t17 = fl_flag(true);
  }
  /* постусловие «смещение внутри пределов остаётся собой» */
  bool fl_t18 = false;
  FL_TRY(fl_post(ctx, fl_t17, "смещение внутри пределов остаётся собой", "Смещение после смены окна", &fl_t18, error));
  if (!fl_t18) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «смещение внутри пределов остаётся собой» функции «Смещение после смены окна»");
  }
  *result = fl_t7;
  return FL_OK;
}

/*
 * Функция flang «Смещение».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param kolonka — «колонка»: число
 * @param shirina — «ширина»: число
 * @param smeschenie — «смещение»: число
 * @param zazor — «зазор»: число
 * @param holst — «холст»: число
 * @return значение: число
 */
fl_status viewport_smeschenie(fl_ctx *ctx, fl_value okno, fl_value kolonka, fl_value shirina, fl_value smeschenie, fl_value zazor, fl_value holst, fl_value *result, fl_error *error) {
  if (shirina.tag != FL_NUMBER || zazor.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", shirina, zazor, error));
  if (okno.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_number((shirina.as.number + zazor.as.number)), okno, error));
  bool fl_t19 = false;
  FL_TRY(fl_cond(ctx, fl_flag((shirina.as.number + zazor.as.number) > okno.as.number), &fl_t19, error));
  fl_value fl_t20 = fl_nothing();
  if (fl_t19) {
    fl_t20 = fl_number(0.0);
  } else {
    fl_t20 = zazor;
  }
  const fl_value otstup = fl_t20; /* пусть «отступ» */
  if (kolonka.tag != FL_NUMBER || shirina.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", kolonka, shirina, error));
  if (otstup.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", fl_number((kolonka.as.number + shirina.as.number)), otstup, error));
  if (smeschenie.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", smeschenie, okno, error));
  bool fl_t21 = false;
  FL_TRY(fl_cond(ctx, fl_flag(((kolonka.as.number + shirina.as.number) + otstup.as.number) > (smeschenie.as.number + okno.as.number)), &fl_t21, error));
  fl_value fl_t22 = fl_nothing();
  if (fl_t21) {
    if (kolonka.tag != FL_NUMBER || shirina.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", kolonka, shirina, error));
    if (otstup.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", fl_number((kolonka.as.number + shirina.as.number)), otstup, error));
    if (okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", fl_number(((kolonka.as.number + shirina.as.number) + otstup.as.number)), okno, error));
    fl_t22 = fl_number(((kolonka.as.number + shirina.as.number) + otstup.as.number) - okno.as.number);
  } else {
    fl_t22 = smeschenie;
  }
  const fl_value kray = fl_t22; /* пусть «край» */
  if (kolonka.tag != FL_NUMBER || otstup.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", kolonka, otstup, error));
  if (smeschenie.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_number((kolonka.as.number - otstup.as.number)), smeschenie, error));
  bool fl_t23 = false;
  FL_TRY(fl_cond(ctx, fl_flag((kolonka.as.number - otstup.as.number) < smeschenie.as.number), &fl_t23, error));
  fl_value fl_t24 = fl_nothing();
  if (fl_t23) {
    if (kolonka.tag != FL_NUMBER || otstup.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", kolonka, otstup, error));
    fl_t24 = fl_number(kolonka.as.number - otstup.as.number);
  } else {
    fl_t24 = kray;
  }
  const fl_value vlevo = fl_t24; /* пусть «влево» */
  if (shirina.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, shirina, okno, error));
  bool fl_t25 = false;
  FL_TRY(fl_cond(ctx, fl_flag(shirina.as.number >= okno.as.number), &fl_t25, error));
  fl_value fl_t26 = fl_nothing();
  if (fl_t25) {
    fl_t26 = kolonka;
  } else {
    fl_t26 = vlevo;
  }
  const fl_value vybor = fl_t26; /* пусть «выбор» */
  fl_value fl_t27 = fl_nothing();
  FL_TRY(viewport_smeschenie_posle_smeny_okna(ctx, okno, vybor, holst, &fl_t27, error));
  const fl_value fl_t28 = fl_t27;
  if (fl_t28.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t28, fl_number(0.0), error));
  /* постусловие «смещение не уходит влево от начала холста» */
  bool fl_t29 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t28.as.number >= 0.0), "смещение не уходит влево от начала холста", "Смещение", &fl_t29, error));
  if (!fl_t29) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «смещение не уходит влево от начала холста» функции «Смещение»");
  }
  if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
  bool fl_t30 = false;
  FL_TRY(fl_cond(ctx, fl_flag((holst.as.number - okno.as.number) < 0.0), &fl_t30, error));
  fl_value fl_t31 = fl_nothing();
  if (fl_t30) {
    fl_t31 = fl_number(0.0);
  } else {
    if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
    fl_t31 = fl_number(holst.as.number - okno.as.number);
  }
  if (fl_t28.tag != FL_NUMBER || fl_t31.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t28, fl_t31, error));
  /* постусловие «смещение не уходит за конец холста» */
  bool fl_t32 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t28.as.number <= fl_t31.as.number), "смещение не уходит за конец холста", "Смещение", &fl_t32, error));
  if (!fl_t32) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «смещение не уходит за конец холста» функции «Смещение»");
  }
  *result = fl_t28;
  return FL_OK;
}

/*
 * Функция flang «Смещение по стопке».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param verh — «верх»: число
 * @param vysota — «высота»: число
 * @param smeschenie — «смещение»: число
 * @param zazor — «зазор»: число
 * @param holst — «холст»: число
 * @return значение: число
 */
fl_status viewport_smeschenie_po_stopke(fl_ctx *ctx, fl_value okno, fl_value verh, fl_value vysota, fl_value smeschenie, fl_value zazor, fl_value holst, fl_value *result, fl_error *error) {
  fl_value fl_t33 = fl_nothing();
  FL_TRY(viewport_smeschenie(ctx, okno, verh, vysota, smeschenie, zazor, holst, &fl_t33, error));
  const fl_value fl_t34 = fl_t33;
  if (fl_t34.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t34, fl_number(0.0), error));
  /* постусловие «смещение не уходит выше начала холста» */
  bool fl_t35 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t34.as.number >= 0.0), "смещение не уходит выше начала холста", "Смещение по стопке", &fl_t35, error));
  if (!fl_t35) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «смещение не уходит выше начала холста» функции «Смещение по стопке»");
  }
  if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
  bool fl_t36 = false;
  FL_TRY(fl_cond(ctx, fl_flag((holst.as.number - okno.as.number) < 0.0), &fl_t36, error));
  fl_value fl_t37 = fl_nothing();
  if (fl_t36) {
    fl_t37 = fl_number(0.0);
  } else {
    if (holst.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", holst, okno, error));
    fl_t37 = fl_number(holst.as.number - okno.as.number);
  }
  if (fl_t34.tag != FL_NUMBER || fl_t37.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t34, fl_t37, error));
  /* постусловие «смещение не уходит ниже конца холста» */
  bool fl_t38 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t34.as.number <= fl_t37.as.number), "смещение не уходит ниже конца холста", "Смещение по стопке", &fl_t38, error));
  if (!fl_t38) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «смещение не уходит ниже конца холста» функции «Смещение по стопке»");
  }
  *result = fl_t34;
  return FL_OK;
}

/*
 * Вызов по исходному имени flang. Коды и тексты — те же, что у
 * интерпретатора: «не найдена функция …» и «функция … принимает N аргум.».
 */
fl_status viewport_call(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error) {
  if (strcmp(name, "Смещение после смены окна") == 0) {
    if (count != 3) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Смещение после смены окна", (unsigned long)3, (unsigned long)count);
    }
    return viewport_smeschenie_posle_smeny_okna(ctx, args[0], args[1], args[2], result, error);
  }
  if (strcmp(name, "Смещение") == 0) {
    if (count != 6) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Смещение", (unsigned long)6, (unsigned long)count);
    }
    return viewport_smeschenie(ctx, args[0], args[1], args[2], args[3], args[4], args[5], result, error);
  }
  if (strcmp(name, "Смещение по стопке") == 0) {
    if (count != 6) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Смещение по стопке", (unsigned long)6, (unsigned long)count);
    }
    return viewport_smeschenie_po_stopke(ctx, args[0], args[1], args[2], args[3], args[4], args[5], result, error);
  }
  return fl_fail(ctx, error, FL_CODE_UNKNOWN_NAME, "не найдена функция «%s»", name);
}

/*
 * ТА ЖЕ ДВЕРЬ, НО С ГРАНИЦЕЙ ВХОДА: сначала объявленные типы параметров
 * (fl_check_entry по таблице внизу файла), потом вызов. Зовите ЭТУ, если
 * значения пришли снаружи — из JSON, из другого языка, от человека.
 *
 * Почему не сверяет сам `_call`. Он обязан отвечать значение в значение так
 * же, как `interpret` у свидетеля, а тот объявленных типов не сверяет тоже:
 * сверяет их `flang run`. Здесь ровно та же пара — `_call` вычислитель,
 * `_enter` дверь, — и разойдись они, у языка стало бы два ответа на вопрос
 * «подходит ли значение типу».
 */
fl_status viewport_enter(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error) {
  FL_TRY(fl_check_entry(ctx, viewport_entry(), name, args, count, error));
  return viewport_call(ctx, name, args, count, result, error);
}

/*
 * Граница входа: объявленные типы параметров данными. Прогонщик сверяет по
 * ним значения, пришедшие снаружи, ДО вызова (fl_check_entry).
 *
 * Виды `неизвестно` (значение-функция, параметр полиморфизма, применение
 * типа с аргументами) не сверяются — ровно как молчит о них проверка
 * значений свидетеля.
 */
static const fl_type viewport_entry_types[] = {
  { FL_TYPE_NUMBER, "число", "", false, false, false, 0.0, 0.0, 0, 0, 0, 0, 0 },
};

static const fl_entry_param viewport_entry_params[] = {
  { "Смещение после смены окна", "окно", 0 },
  { "Смещение после смены окна", "смещение", 0 },
  { "Смещение после смены окна", "холст", 0 },
  { "Смещение", "окно", 0 },
  { "Смещение", "колонка", 0 },
  { "Смещение", "ширина", 0 },
  { "Смещение", "смещение", 0 },
  { "Смещение", "зазор", 0 },
  { "Смещение", "холст", 0 },
  { "Смещение по стопке", "окно", 0 },
  { "Смещение по стопке", "верх", 0 },
  { "Смещение по стопке", "высота", 0 },
  { "Смещение по стопке", "смещение", 0 },
  { "Смещение по стопке", "зазор", 0 },
  { "Смещение по стопке", "холст", 0 },
};

static const fl_entry_table viewport_entry_table = {
  viewport_entry_types, 1,
  NULL, 0,
  NULL, 0,
  viewport_entry_params, 15
};

const fl_entry_table *viewport_entry(void) {
  return &viewport_entry_table;
}
