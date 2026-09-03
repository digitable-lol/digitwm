/*
 * Сгенерировано flang (бэкенд C, flang/self/emit-c.flang). Не редактировать руками.
 * Модуль flang: «Strut».
 * Файл: реализация.
 * Правьте исходник на flang и печатайте заново: любая правка здесь потеряется.
 */
#include "strut.h"

#include <string.h>


/*
 * Функция flang «Полоса встречает область».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param nachalo — «начало»: число
 * @param konec — «конец»: число
 * @param polozhenie — «положение»: число
 * @param oblast — «область»: число
 * @return значение: число
 */
fl_status strut_polosa_vstrechaet_oblast(fl_ctx *ctx, fl_value nachalo, fl_value konec, fl_value polozhenie, fl_value oblast, fl_value *result, fl_error *error) {
  if (polozhenie.tag != FL_NUMBER || oblast.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", polozhenie, oblast, error));
  if (nachalo.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, nachalo, fl_number(((polozhenie.as.number + oblast.as.number) - 1.0)), error));
  bool fl_t1 = false;
  FL_TRY(fl_cond(ctx, fl_flag(nachalo.as.number <= ((polozhenie.as.number + oblast.as.number) - 1.0)), &fl_t1, error));
  fl_value fl_t2 = fl_nothing();
  if (fl_t1) {
    if (konec.tag != FL_NUMBER || polozhenie.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, konec, polozhenie, error));
    fl_t2 = fl_flag(konec.as.number >= polozhenie.as.number);
  } else {
    fl_t2 = fl_flag(false);
  }
  bool fl_t3 = false;
  FL_TRY(fl_cond(ctx, fl_t2, &fl_t3, error));
  fl_value fl_t4 = fl_nothing();
  if (fl_t3) {
    fl_t4 = fl_number(1.0);
  } else {
    fl_t4 = fl_number(0.0);
  }
  const fl_value vstrecha = fl_t4; /* пусть «встреча» */
  if (oblast.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, oblast, fl_number(0.0), error));
  bool fl_t5 = false;
  FL_TRY(fl_cond(ctx, fl_flag(oblast.as.number <= 0.0), &fl_t5, error));
  fl_value fl_t6 = fl_nothing();
  if (fl_t5) {
    fl_t6 = fl_number(0.0);
  } else {
    fl_t6 = vstrecha;
  }
  const fl_value zhivaya = fl_t6; /* пусть «живая» */
  if (konec.tag != FL_NUMBER || nachalo.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, konec, nachalo, error));
  bool fl_t7 = false;
  FL_TRY(fl_cond(ctx, fl_flag(konec.as.number < nachalo.as.number), &fl_t7, error));
  if (fl_t7) {
    *result = fl_number(0.0);
    return FL_OK;
  } else {
    *result = zhivaya;
    return FL_OK;
  }
}

/*
 * Функция flang «Сколько отнять».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param panel — «панель»: число
 * @param ekran — «экран»: число
 * @param polozhenie — «положение»: число
 * @param oblast — «область»: число
 * @param dalniy — «дальний»: число
 * @return значение: число
 */
fl_status strut_skolko_otnyat(fl_ctx *ctx, fl_value panel, fl_value ekran, fl_value polozhenie, fl_value oblast, fl_value dalniy, fl_value *result, fl_error *error) {
  bool fl_t8 = false;
  FL_TRY(fl_cond(ctx, fl_flag(!fl_equal(dalniy, fl_number(0.0))), &fl_t8, error));
  fl_value fl_t9 = fl_nothing();
  if (fl_t8) {
    if (polozhenie.tag != FL_NUMBER || oblast.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", polozhenie, oblast, error));
    if (ekran.tag != FL_NUMBER || panel.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", ekran, panel, error));
    fl_t9 = fl_number((polozhenie.as.number + oblast.as.number) - (ekran.as.number - panel.as.number));
  } else {
    if (panel.tag != FL_NUMBER || polozhenie.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", panel, polozhenie, error));
    fl_t9 = fl_number(panel.as.number - polozhenie.as.number);
  }
  const fl_value syroe = fl_t9; /* пусть «сырое» */
  if (syroe.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, syroe, fl_number(0.0), error));
  bool fl_t10 = false;
  FL_TRY(fl_cond(ctx, fl_flag(syroe.as.number < 0.0), &fl_t10, error));
  fl_value fl_t11 = fl_nothing();
  if (fl_t10) {
    fl_t11 = fl_number(0.0);
  } else {
    fl_t11 = syroe;
  }
  const fl_value snizu = fl_t11; /* пусть «снизу» */
  if (snizu.tag != FL_NUMBER || oblast.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, snizu, oblast, error));
  bool fl_t12 = false;
  FL_TRY(fl_cond(ctx, fl_flag(snizu.as.number > oblast.as.number), &fl_t12, error));
  fl_value fl_t13 = fl_nothing();
  if (fl_t12) {
    fl_t13 = oblast;
  } else {
    fl_t13 = snizu;
  }
  const fl_value sverhu = fl_t13; /* пусть «сверху» */
  if (panel.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, panel, fl_number(0.0), error));
  bool fl_t14 = false;
  FL_TRY(fl_cond(ctx, fl_flag(panel.as.number <= 0.0), &fl_t14, error));
  fl_value fl_t15 = fl_nothing();
  if (fl_t14) {
    fl_t15 = fl_number(0.0);
  } else {
    fl_t15 = sverhu;
  }
  const fl_value fl_t16 = fl_t15;
  if (oblast.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, oblast, fl_number(0.0), error));
  bool fl_t17 = false;
  FL_TRY(fl_cond(ctx, fl_flag(oblast.as.number >= 0.0), &fl_t17, error));
  fl_value fl_t18 = fl_nothing();
  if (fl_t17) {
    if (fl_t16.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t16, fl_number(0.0), error));
    bool fl_t19 = false;
    FL_TRY(fl_cond(ctx, fl_flag(fl_t16.as.number >= 0.0), &fl_t19, error));
    fl_value fl_t20 = fl_nothing();
    if (fl_t19) {
      if (fl_t16.tag != FL_NUMBER || oblast.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t16, oblast, error));
      fl_t20 = fl_flag(fl_t16.as.number <= oblast.as.number);
    } else {
      fl_t20 = fl_flag(false);
    }
    fl_t18 = fl_t20;
  } else {
    fl_t18 = fl_flag(true);
  }
  /* постусловие «у неотрицательной области отнятое лежит между нулём и ней» */
  bool fl_t21 = false;
  FL_TRY(fl_post(ctx, fl_t18, "у неотрицательной области отнятое лежит между нулём и ней", "Сколько отнять", &fl_t21, error));
  if (!fl_t21) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «у неотрицательной области отнятое лежит между нулём и ней» функции «Сколько отнять»");
  }
  *result = fl_t16;
  return FL_OK;
}

/*
 * Функция flang «Доля пары».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param blizhnyaya — «ближняя»: число
 * @param dalnyaya — «дальняя»: число
 * @param oblast — «область»: число
 * @param sprashivayut — «спрашивают»: число
 * @return значение: число
 */
fl_status strut_dolya_pary(fl_ctx *ctx, fl_value blizhnyaya, fl_value dalnyaya, fl_value oblast, fl_value sprashivayut, fl_value *result, fl_error *error) {
  if (oblast.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, oblast, fl_number(0.0), error));
  bool fl_t22 = false;
  FL_TRY(fl_cond(ctx, fl_flag(oblast.as.number < 0.0), &fl_t22, error));
  fl_value fl_t23 = fl_nothing();
  if (fl_t22) {
    fl_t23 = fl_number(0.0);
  } else {
    fl_t23 = oblast;
  }
  const fl_value dlina = fl_t23; /* пусть «длина» */
  if (blizhnyaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, blizhnyaya, fl_number(0.0), error));
  bool fl_t24 = false;
  FL_TRY(fl_cond(ctx, fl_flag(blizhnyaya.as.number < 0.0), &fl_t24, error));
  fl_value fl_t25 = fl_nothing();
  if (fl_t24) {
    fl_t25 = fl_number(0.0);
  } else {
    fl_t25 = blizhnyaya;
  }
  const fl_value pervaya = fl_t25; /* пусть «первая» */
  if (dalnyaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, dalnyaya, fl_number(0.0), error));
  bool fl_t26 = false;
  FL_TRY(fl_cond(ctx, fl_flag(dalnyaya.as.number < 0.0), &fl_t26, error));
  fl_value fl_t27 = fl_nothing();
  if (fl_t26) {
    fl_t27 = fl_number(0.0);
  } else {
    fl_t27 = dalnyaya;
  }
  const fl_value vtoraya = fl_t27; /* пусть «вторая» */
  if (pervaya.tag != FL_NUMBER || dlina.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, pervaya, dlina, error));
  bool fl_t28 = false;
  FL_TRY(fl_cond(ctx, fl_flag(pervaya.as.number > dlina.as.number), &fl_t28, error));
  fl_value fl_t29 = fl_nothing();
  if (fl_t28) {
    fl_t29 = dlina;
  } else {
    fl_t29 = pervaya;
  }
  const fl_value zazhataya = fl_t29; /* пусть «зажатая» */
  bool fl_t30 = false;
  FL_TRY(fl_cond(ctx, fl_flag(!fl_equal(sprashivayut, fl_number(0.0))), &fl_t30, error));
  fl_value fl_t31 = fl_nothing();
  if (fl_t30) {
    if (dlina.tag != FL_NUMBER || zazhataya.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", dlina, zazhataya, error));
    fl_t31 = fl_number(dlina.as.number - zazhataya.as.number);
  } else {
    fl_t31 = zazhataya;
  }
  const fl_value tesno = fl_t31; /* пусть «тесно» */
  bool fl_t32 = false;
  FL_TRY(fl_cond(ctx, fl_flag(!fl_equal(sprashivayut, fl_number(0.0))), &fl_t32, error));
  fl_value fl_t33 = fl_nothing();
  if (fl_t32) {
    fl_t33 = vtoraya;
  } else {
    fl_t33 = pervaya;
  }
  const fl_value prostorno = fl_t33; /* пусть «просторно» */
  if (pervaya.tag != FL_NUMBER || vtoraya.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", pervaya, vtoraya, error));
  if (dlina.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_number((pervaya.as.number + vtoraya.as.number)), dlina, error));
  bool fl_t34 = false;
  FL_TRY(fl_cond(ctx, fl_flag((pervaya.as.number + vtoraya.as.number) <= dlina.as.number), &fl_t34, error));
  fl_value fl_t35 = fl_nothing();
  if (fl_t34) {
    fl_t35 = prostorno;
  } else {
    fl_t35 = tesno;
  }
  const fl_value fl_t36 = fl_t35;
  if (fl_t36.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t36, fl_number(0.0), error));
  /* постусловие «доля не отрицательна» */
  bool fl_t37 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t36.as.number >= 0.0), "доля не отрицательна", "Доля пары", &fl_t37, error));
  if (!fl_t37) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «доля не отрицательна» функции «Доля пары»");
  }
  if (oblast.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, oblast, fl_number(0.0), error));
  bool fl_t38 = false;
  FL_TRY(fl_cond(ctx, fl_flag(oblast.as.number < 0.0), &fl_t38, error));
  fl_value fl_t39 = fl_nothing();
  if (fl_t38) {
    fl_t39 = fl_number(0.0);
  } else {
    fl_t39 = oblast;
  }
  if (fl_t36.tag != FL_NUMBER || fl_t39.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t36, fl_t39, error));
  /* постусловие «доля не больше области» */
  bool fl_t40 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t36.as.number <= fl_t39.as.number), "доля не больше области", "Доля пары", &fl_t40, error));
  if (!fl_t40) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «доля не больше области» функции «Доля пары»");
  }
  *result = fl_t36;
  return FL_OK;
}

/*
 * Функция flang «Пара вместе».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param blizhnyaya — «ближняя»: число
 * @param dalnyaya — «дальняя»: число
 * @param oblast — «область»: число
 * @return значение: число
 */
fl_status strut_para_vmeste(fl_ctx *ctx, fl_value blizhnyaya, fl_value dalnyaya, fl_value oblast, fl_value *result, fl_error *error) {
  fl_value fl_t41 = fl_nothing();
  FL_TRY(strut_dolya_pary(ctx, blizhnyaya, dalnyaya, oblast, fl_number(0.0), &fl_t41, error));
  fl_value fl_t42 = fl_nothing();
  FL_TRY(strut_dolya_pary(ctx, blizhnyaya, dalnyaya, oblast, fl_number(1.0), &fl_t42, error));
  if (fl_t41.tag != FL_NUMBER || fl_t42.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", fl_t41, fl_t42, error));
  const fl_value fl_t43 = fl_number(fl_t41.as.number + fl_t42.as.number);
  if (oblast.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, oblast, fl_number(0.0), error));
  bool fl_t44 = false;
  FL_TRY(fl_cond(ctx, fl_flag(oblast.as.number < 0.0), &fl_t44, error));
  fl_value fl_t45 = fl_nothing();
  if (fl_t44) {
    fl_t45 = fl_number(0.0);
  } else {
    fl_t45 = oblast;
  }
  if (fl_t43.tag != FL_NUMBER || fl_t45.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t43, fl_t45, error));
  /* постусловие «вдвоём панели не берут больше области» */
  bool fl_t46 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t43.as.number <= fl_t45.as.number), "вдвоём панели не берут больше области", "Пара вместе", &fl_t46, error));
  if (!fl_t46) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «вдвоём панели не берут больше области» функции «Пара вместе»");
  }
  if (fl_t43.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t43, fl_number(0.0), error));
  /* постусловие «вдвоём панели не берут отрицательного» */
  bool fl_t47 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t43.as.number >= 0.0), "вдвоём панели не берут отрицательного", "Пара вместе", &fl_t47, error));
  if (!fl_t47) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «вдвоём панели не берут отрицательного» функции «Пара вместе»");
  }
  *result = fl_t43;
  return FL_OK;
}

/*
 * Вызов по исходному имени flang. Коды и тексты — те же, что у
 * интерпретатора: «не найдена функция …» и «функция … принимает N аргум.».
 */
fl_status strut_call(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error) {
  if (strcmp(name, "Полоса встречает область") == 0) {
    if (count != 4) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Полоса встречает область", (unsigned long)4, (unsigned long)count);
    }
    return strut_polosa_vstrechaet_oblast(ctx, args[0], args[1], args[2], args[3], result, error);
  }
  if (strcmp(name, "Сколько отнять") == 0) {
    if (count != 5) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Сколько отнять", (unsigned long)5, (unsigned long)count);
    }
    return strut_skolko_otnyat(ctx, args[0], args[1], args[2], args[3], args[4], result, error);
  }
  if (strcmp(name, "Доля пары") == 0) {
    if (count != 4) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Доля пары", (unsigned long)4, (unsigned long)count);
    }
    return strut_dolya_pary(ctx, args[0], args[1], args[2], args[3], result, error);
  }
  if (strcmp(name, "Пара вместе") == 0) {
    if (count != 3) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Пара вместе", (unsigned long)3, (unsigned long)count);
    }
    return strut_para_vmeste(ctx, args[0], args[1], args[2], result, error);
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
fl_status strut_enter(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error) {
  FL_TRY(fl_check_entry(ctx, strut_entry(), name, args, count, error));
  return strut_call(ctx, name, args, count, result, error);
}

/*
 * Граница входа: объявленные типы параметров данными. Прогонщик сверяет по
 * ним значения, пришедшие снаружи, ДО вызова (fl_check_entry).
 *
 * Виды `неизвестно` (значение-функция, параметр полиморфизма, применение
 * типа с аргументами) не сверяются — ровно как молчит о них проверка
 * значений свидетеля.
 */
static const fl_type strut_entry_types[] = {
  { FL_TYPE_NUMBER, "число", "", false, false, false, 0.0, 0.0, 0, 0, 0, 0, 0 },
};

static const fl_entry_param strut_entry_params[] = {
  { "Полоса встречает область", "начало", 0 },
  { "Полоса встречает область", "конец", 0 },
  { "Полоса встречает область", "положение", 0 },
  { "Полоса встречает область", "область", 0 },
  { "Сколько отнять", "панель", 0 },
  { "Сколько отнять", "экран", 0 },
  { "Сколько отнять", "положение", 0 },
  { "Сколько отнять", "область", 0 },
  { "Сколько отнять", "дальний", 0 },
  { "Доля пары", "ближняя", 0 },
  { "Доля пары", "дальняя", 0 },
  { "Доля пары", "область", 0 },
  { "Доля пары", "спрашивают", 0 },
  { "Пара вместе", "ближняя", 0 },
  { "Пара вместе", "дальняя", 0 },
  { "Пара вместе", "область", 0 },
};

static const fl_entry_table strut_entry_table = {
  strut_entry_types, 1,
  NULL, 0,
  NULL, 0,
  strut_entry_params, 16
};

const fl_entry_table *strut_entry(void) {
  return &strut_entry_table;
}
