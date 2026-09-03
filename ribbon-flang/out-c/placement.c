/*
 * Сгенерировано flang (бэкенд C, flang/self/emit-c.flang). Не редактировать руками.
 * Модуль flang: «Placement».
 * Файл: реализация.
 * Правьте исходник на flang и печатайте заново: любая правка здесь потеряется.
 */
#include "placement.h"

#include <string.h>


/*
 * Функция flang «Место: своя колонка».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_mesto_svoya_kolonka(fl_ctx *ctx, fl_value *result, fl_error *error) {
  (void)ctx;
  (void)error;
  *result = fl_number(0.0);
  return FL_OK;
}

/*
 * Функция flang «Место: в стопку».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_mesto_v_stopku(fl_ctx *ctx, fl_value *result, fl_error *error) {
  (void)ctx;
  (void)error;
  *result = fl_number(1.0);
  return FL_OK;
}

/*
 * Функция flang «Место: плавающее».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_mesto_plavayuschee(fl_ctx *ctx, fl_value *result, fl_error *error) {
  (void)ctx;
  (void)error;
  *result = fl_number(2.0);
  return FL_OK;
}

/*
 * Функция flang «Место: во весь экран».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_mesto_vo_ves_ekran(fl_ctx *ctx, fl_value *result, fl_error *error) {
  (void)ctx;
  (void)error;
  *result = fl_number(3.0);
  return FL_OK;
}

/*
 * Функция flang «Правило: без правила».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_pravilo_bez_pravila(fl_ctx *ctx, fl_value *result, fl_error *error) {
  (void)ctx;
  (void)error;
  *result = fl_number(0.0);
  return FL_OK;
}

/*
 * Функция flang «Правило: в стопку».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_pravilo_v_stopku(fl_ctx *ctx, fl_value *result, fl_error *error) {
  (void)ctx;
  (void)error;
  *result = fl_number(1.0);
  return FL_OK;
}

/*
 * Функция flang «Правило: плавающее».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @return значение: число
 */
fl_status placement_pravilo_plavayuschee(fl_ctx *ctx, fl_value *result, fl_error *error) {
  (void)ctx;
  (void)error;
  *result = fl_number(2.0);
  return FL_OK;
}

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
fl_status placement_kuda_polozhit_okno(fl_ctx *ctx, fl_value fokus, fl_value dochernee, fl_value dialog, fl_value dok, fl_value voves, fl_value ukazanie, fl_value *result, fl_error *error) {
  bool fl_t1 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(ukazanie, fl_number(1.0))), &fl_t1, error));
  fl_value fl_t2 = fl_nothing();
  if (fl_t1) {
    fl_t2 = fl_flag(!fl_equal(fokus, fl_number(0.0)));
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
  const fl_value vstopku = fl_t4; /* пусть «встопку» */
  bool fl_t5 = false;
  FL_TRY(fl_cond(ctx, fl_flag(!fl_equal(voves, fl_number(0.0))), &fl_t5, error));
  fl_value fl_t6 = fl_nothing();
  if (fl_t5) {
    fl_t6 = fl_number(3.0);
  } else {
    fl_t6 = vstopku;
  }
  const fl_value naves = fl_t6; /* пусть «навесь» */
  bool fl_t7 = false;
  FL_TRY(fl_cond(ctx, fl_flag(!fl_equal(dialog, fl_number(0.0))), &fl_t7, error));
  fl_value fl_t8 = fl_nothing();
  if (fl_t7) {
    fl_t8 = fl_flag(true);
  } else {
    fl_t8 = fl_flag(!fl_equal(dochernee, fl_number(0.0)));
  }
  bool fl_t9 = false;
  FL_TRY(fl_cond(ctx, fl_t8, &fl_t9, error));
  fl_value fl_t10 = fl_nothing();
  if (fl_t9) {
    fl_t10 = fl_number(2.0);
  } else {
    fl_t10 = naves;
  }
  const fl_value dialogovoe = fl_t10; /* пусть «диалоговое» */
  bool fl_t11 = false;
  FL_TRY(fl_cond(ctx, fl_flag(!fl_equal(dok, fl_number(0.0))), &fl_t11, error));
  fl_value fl_t12 = fl_nothing();
  if (fl_t11) {
    fl_t12 = fl_number(2.0);
  } else {
    fl_t12 = dialogovoe;
  }
  const fl_value dokovoe = fl_t12; /* пусть «доковое» */
  bool fl_t13 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(ukazanie, fl_number(2.0))), &fl_t13, error));
  fl_value fl_t14 = fl_nothing();
  if (fl_t13) {
    fl_t14 = fl_number(2.0);
  } else {
    fl_t14 = dokovoe;
  }
  const fl_value fl_t15 = fl_t14;
  bool fl_t16 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(fl_t15, fl_number(0.0))), &fl_t16, error));
  fl_value fl_t17 = fl_nothing();
  if (fl_t16) {
    fl_t17 = fl_flag(true);
  } else {
    fl_t17 = fl_flag(fl_equal(fl_t15, fl_number(1.0)));
  }
  bool fl_t18 = false;
  FL_TRY(fl_cond(ctx, fl_t17, &fl_t18, error));
  fl_value fl_t19 = fl_nothing();
  if (fl_t18) {
    fl_t19 = fl_flag(true);
  } else {
    fl_t19 = fl_flag(fl_equal(fl_t15, fl_number(2.0)));
  }
  bool fl_t20 = false;
  FL_TRY(fl_cond(ctx, fl_t19, &fl_t20, error));
  fl_value fl_t21 = fl_nothing();
  if (fl_t20) {
    fl_t21 = fl_flag(true);
  } else {
    fl_t21 = fl_flag(fl_equal(fl_t15, fl_number(3.0)));
  }
  /* постусловие «место — одно из четырёх» */
  bool fl_t22 = false;
  FL_TRY(fl_post(ctx, fl_t21, "место — одно из четырёх", "Куда положить окно", &fl_t22, error));
  if (!fl_t22) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «место — одно из четырёх» функции «Куда положить окно»");
  }
  *result = fl_t15;
  return FL_OK;
}

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
fl_status placement_fokus_posle_zakrytiya(fl_ctx *ctx, fl_value nomer, fl_value kolonok, fl_value poslednyaya, fl_value odno, fl_value *result, fl_error *error) {
  if (kolonok.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", kolonok, fl_number(1.0), error));
  const fl_value ostalos = fl_number(kolonok.as.number - 1.0); /* пусть «осталось» */
  bool fl_t23 = false;
  FL_TRY(fl_cond(ctx, fl_flag(!fl_equal(poslednyaya, fl_number(0.0))), &fl_t23, error));
  fl_value fl_t24 = fl_nothing();
  if (fl_t23) {
    fl_t24 = fl_flag(true);
  } else {
    if (nomer.tag != FL_NUMBER || ostalos.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, nomer, ostalos, error));
    fl_t24 = fl_flag(nomer.as.number >= ostalos.as.number);
  }
  bool fl_t25 = false;
  FL_TRY(fl_cond(ctx, fl_t24, &fl_t25, error));
  fl_value fl_t26 = fl_nothing();
  if (fl_t25) {
    if (ostalos.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", ostalos, fl_number(1.0), error));
    fl_t26 = fl_number(ostalos.as.number - 1.0);
  } else {
    fl_t26 = nomer;
  }
  const fl_value vpravo = fl_t26; /* пусть «вправо» */
  if (ostalos.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, ostalos, fl_number(0.0), error));
  bool fl_t27 = false;
  FL_TRY(fl_cond(ctx, fl_flag(ostalos.as.number <= 0.0), &fl_t27, error));
  fl_value fl_t28 = fl_nothing();
  if (fl_t27) {
    fl_t28 = fl_number(0.0);
  } else {
    fl_t28 = vpravo;
  }
  const fl_value novaya = fl_t28; /* пусть «новая» */
  bool fl_t29 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(odno, fl_number(0.0))), &fl_t29, error));
  fl_value fl_t30 = fl_nothing();
  if (fl_t29) {
    fl_t30 = nomer;
  } else {
    fl_t30 = novaya;
  }
  const fl_value fl_t31 = fl_t30;
  bool fl_t32 = false;
  FL_TRY(fl_cond(ctx, fl_flag(!fl_equal(odno, fl_number(0.0))), &fl_t32, error));
  fl_value fl_t33 = fl_nothing();
  if (fl_t32) {
    if (nomer.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, nomer, fl_number(0.0), error));
    fl_t33 = fl_flag(nomer.as.number >= 0.0);
  } else {
    fl_t33 = fl_flag(false);
  }
  bool fl_t34 = false;
  FL_TRY(fl_cond(ctx, fl_t33, &fl_t34, error));
  fl_value fl_t35 = fl_nothing();
  if (fl_t34) {
    if (fl_t31.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t31, fl_number(0.0), error));
    fl_t35 = fl_flag(fl_t31.as.number >= 0.0);
  } else {
    fl_t35 = fl_flag(true);
  }
  /* постусловие «колонка уходит — фокус не отрицателен, если номер был не отрицателен» */
  bool fl_t36 = false;
  FL_TRY(fl_post(ctx, fl_t35, "колонка уходит — фокус не отрицателен, если номер был не отрицателен", "Фокус после закрытия", &fl_t36, error));
  if (!fl_t36) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «колонка уходит — фокус не отрицателен, если номер был не отрицателен» функции «Фокус после закрытия»");
  }
  *result = fl_t31;
  return FL_OK;
}

/*
 * Вызов по исходному имени flang. Коды и тексты — те же, что у
 * интерпретатора: «не найдена функция …» и «функция … принимает N аргум.».
 */
fl_status placement_call(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error) {
  if (strcmp(name, "Место: своя колонка") == 0) {
    if (count != 0) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Место: своя колонка", (unsigned long)0, (unsigned long)count);
    }
    return placement_mesto_svoya_kolonka(ctx, result, error);
  }
  if (strcmp(name, "Место: в стопку") == 0) {
    if (count != 0) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Место: в стопку", (unsigned long)0, (unsigned long)count);
    }
    return placement_mesto_v_stopku(ctx, result, error);
  }
  if (strcmp(name, "Место: плавающее") == 0) {
    if (count != 0) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Место: плавающее", (unsigned long)0, (unsigned long)count);
    }
    return placement_mesto_plavayuschee(ctx, result, error);
  }
  if (strcmp(name, "Место: во весь экран") == 0) {
    if (count != 0) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Место: во весь экран", (unsigned long)0, (unsigned long)count);
    }
    return placement_mesto_vo_ves_ekran(ctx, result, error);
  }
  if (strcmp(name, "Правило: без правила") == 0) {
    if (count != 0) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Правило: без правила", (unsigned long)0, (unsigned long)count);
    }
    return placement_pravilo_bez_pravila(ctx, result, error);
  }
  if (strcmp(name, "Правило: в стопку") == 0) {
    if (count != 0) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Правило: в стопку", (unsigned long)0, (unsigned long)count);
    }
    return placement_pravilo_v_stopku(ctx, result, error);
  }
  if (strcmp(name, "Правило: плавающее") == 0) {
    if (count != 0) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Правило: плавающее", (unsigned long)0, (unsigned long)count);
    }
    return placement_pravilo_plavayuschee(ctx, result, error);
  }
  if (strcmp(name, "Куда положить окно") == 0) {
    if (count != 6) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Куда положить окно", (unsigned long)6, (unsigned long)count);
    }
    return placement_kuda_polozhit_okno(ctx, args[0], args[1], args[2], args[3], args[4], args[5], result, error);
  }
  if (strcmp(name, "Фокус после закрытия") == 0) {
    if (count != 4) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Фокус после закрытия", (unsigned long)4, (unsigned long)count);
    }
    return placement_fokus_posle_zakrytiya(ctx, args[0], args[1], args[2], args[3], result, error);
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
fl_status placement_enter(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error) {
  FL_TRY(fl_check_entry(ctx, placement_entry(), name, args, count, error));
  return placement_call(ctx, name, args, count, result, error);
}

/*
 * Граница входа: объявленные типы параметров данными. Прогонщик сверяет по
 * ним значения, пришедшие снаружи, ДО вызова (fl_check_entry).
 *
 * Виды `неизвестно` (значение-функция, параметр полиморфизма, применение
 * типа с аргументами) не сверяются — ровно как молчит о них проверка
 * значений свидетеля.
 */
static const fl_type placement_entry_types[] = {
  { FL_TYPE_NUMBER, "число", "", false, false, false, 0.0, 0.0, 0, 0, 0, 0, 0 },
};

static const fl_entry_param placement_entry_params[] = {
  { "Куда положить окно", "фокус", 0 },
  { "Куда положить окно", "дочернее", 0 },
  { "Куда положить окно", "диалог", 0 },
  { "Куда положить окно", "док", 0 },
  { "Куда положить окно", "вовесь", 0 },
  { "Куда положить окно", "указание", 0 },
  { "Фокус после закрытия", "номер", 0 },
  { "Фокус после закрытия", "колонок", 0 },
  { "Фокус после закрытия", "последняя", 0 },
  { "Фокус после закрытия", "одно", 0 },
};

static const fl_entry_table placement_entry_table = {
  placement_entry_types, 1,
  NULL, 0,
  NULL, 0,
  placement_entry_params, 10
};

const fl_entry_table *placement_entry(void) {
  return &placement_entry_table;
}
