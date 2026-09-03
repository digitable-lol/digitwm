/*
 * Сгенерировано flang (бэкенд C, flang/self/emit-c.flang). Не редактировать руками.
 * Модуль flang: «Geometry».
 * Файл: реализация.
 * Правьте исходник на flang и печатайте заново: любая правка здесь потеряется.
 */
#include "geometry.h"

#include <string.h>
#include <math.h> /* fmod, NAN и INFINITY: печать зовёт их по имени */


/*
 * Функция flang «Деление нацело».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param delimoe — «делимое»: число
 * @param delitel — «делитель»: число
 * @return значение: число
 */
fl_status geometry_delenie_nacelo(fl_ctx *ctx, fl_value delimoe, fl_value delitel, fl_value *result, fl_error *error) {
  bool fl_t1 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(delitel, fl_number(0.0))), &fl_t1, error));
  fl_value fl_t2 = fl_nothing();
  if (fl_t1) {
    fl_t2 = fl_number(0.0);
  } else {
    if (delimoe.tag != FL_NUMBER || delitel.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "mod", delimoe, delitel, error));
    if (delimoe.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", delimoe, fl_number((fmod(delimoe.as.number, delitel.as.number))), error));
    if (delitel.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "div", fl_number((delimoe.as.number - (fmod(delimoe.as.number, delitel.as.number)))), delitel, error));
    fl_t2 = fl_number((delimoe.as.number - (fmod(delimoe.as.number, delitel.as.number))) / delitel.as.number);
  }
  const fl_value fl_t3 = fl_t2;
  bool fl_t4 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(delitel, fl_number(0.0))), &fl_t4, error));
  fl_value fl_t5 = fl_nothing();
  if (fl_t4) {
    fl_t5 = fl_flag(true);
  } else {
    if (fl_t3.tag != FL_NUMBER || delitel.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "mul", fl_t3, delitel, error));
    if (delimoe.tag != FL_NUMBER || delitel.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "mod", delimoe, delitel, error));
    if (delimoe.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "add", delimoe, fl_number(0.0), error));
    fl_t5 = fl_flag(fl_equal(fl_number(((fl_t3.as.number * delitel.as.number) + (fmod(delimoe.as.number, delitel.as.number))) + 0.0), fl_number(delimoe.as.number + 0.0)));
  }
  /* постусловие «частное с остатком восстанавливают делимое» */
  bool fl_t6 = false;
  FL_TRY(fl_post(ctx, fl_t5, "частное с остатком восстанавливают делимое", "Деление нацело", &fl_t6, error));
  if (!fl_t6) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «частное с остатком восстанавливают делимое» функции «Деление нацело»");
  }
  *result = fl_t3;
  return FL_OK;
}

/*
 * Функция flang «Пресет в пределах».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param preset — «пресет»: число
 * @return значение: число
 */
fl_status geometry_preset_v_predelah(fl_ctx *ctx, fl_value preset, fl_value *result, fl_error *error) {
  if (preset.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, preset, fl_number(0.0), error));
  bool fl_t7 = false;
  FL_TRY(fl_cond(ctx, fl_flag(preset.as.number < 0.0), &fl_t7, error));
  fl_value fl_t8 = fl_nothing();
  if (fl_t7) {
    fl_t8 = fl_number(0.0);
  } else {
    if (preset.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, preset, fl_number(4.0), error));
    bool fl_t9 = false;
    FL_TRY(fl_cond(ctx, fl_flag(preset.as.number >= 4.0), &fl_t9, error));
    fl_value fl_t10 = fl_nothing();
    if (fl_t9) {
      fl_t10 = fl_number(3.0);
    } else {
      fl_t10 = preset;
    }
    fl_t8 = fl_t10;
  }
  const fl_value fl_t11 = fl_t8;
  if (fl_t11.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t11, fl_number(0.0), error));
  bool fl_t12 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_t11.as.number >= 0.0), &fl_t12, error));
  fl_value fl_t13 = fl_nothing();
  if (fl_t12) {
    if (fl_t11.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t11, fl_number(3.0), error));
    fl_t13 = fl_flag(fl_t11.as.number <= 3.0);
  } else {
    fl_t13 = fl_flag(false);
  }
  /* постусловие «пресет попадает в таблицу из четырёх» */
  bool fl_t14 = false;
  FL_TRY(fl_post(ctx, fl_t13, "пресет попадает в таблицу из четырёх", "Пресет в пределах", &fl_t14, error));
  if (!fl_t14) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «пресет попадает в таблицу из четырёх» функции «Пресет в пределах»");
  }
  *result = fl_t11;
  return FL_OK;
}

/*
 * Функция flang «Ширина колонки».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param dolya — «доля»: число
 * @param zazor — «зазор»: число
 * @param naimenshaya — «наименьшая»: число
 * @return значение: число
 */
fl_status geometry_shirina_kolonki(fl_ctx *ctx, fl_value okno, fl_value dolya, fl_value zazor, fl_value naimenshaya, fl_value *result, fl_error *error) {
  if (dolya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, dolya, fl_number(100.0), error));
  bool fl_t15 = false;
  FL_TRY(fl_cond(ctx, fl_flag(dolya.as.number >= 100.0), &fl_t15, error));
  fl_value fl_t16 = fl_nothing();
  if (fl_t15) {
    fl_t16 = okno;
  } else {
    if (okno.tag != FL_NUMBER || zazor.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", okno, zazor, error));
    if (dolya.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "mul", fl_number((okno.as.number - zazor.as.number)), dolya, error));
    fl_value fl_t17 = fl_nothing();
    FL_TRY(geometry_delenie_nacelo(ctx, fl_number((okno.as.number - zazor.as.number) * dolya.as.number), fl_number(100.0), &fl_t17, error));
    fl_t16 = fl_t17;
  }
  const fl_value syraya = fl_t16; /* пусть «сырая» */
  if (okno.tag != FL_NUMBER || naimenshaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, okno, naimenshaya, error));
  bool fl_t18 = false;
  FL_TRY(fl_cond(ctx, fl_flag(okno.as.number > naimenshaya.as.number), &fl_t18, error));
  fl_value fl_t19 = fl_nothing();
  if (fl_t18) {
    fl_t19 = okno;
  } else {
    fl_t19 = naimenshaya;
  }
  const fl_value predel = fl_t19; /* пусть «предел» */
  if (syraya.tag != FL_NUMBER || predel.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, syraya, predel, error));
  bool fl_t20 = false;
  FL_TRY(fl_cond(ctx, fl_flag(syraya.as.number > predel.as.number), &fl_t20, error));
  fl_value fl_t21 = fl_nothing();
  if (fl_t20) {
    fl_t21 = predel;
  } else {
    fl_t21 = syraya;
  }
  const fl_value sverhu = fl_t21; /* пусть «сверху» */
  if (sverhu.tag != FL_NUMBER || naimenshaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, sverhu, naimenshaya, error));
  bool fl_t22 = false;
  FL_TRY(fl_cond(ctx, fl_flag(sverhu.as.number < naimenshaya.as.number), &fl_t22, error));
  fl_value fl_t23 = fl_nothing();
  if (fl_t22) {
    fl_t23 = naimenshaya;
  } else {
    fl_t23 = sverhu;
  }
  const fl_value fl_t24 = fl_t23;
  if (fl_t24.tag != FL_NUMBER || naimenshaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t24, naimenshaya, error));
  /* постусловие «ширина не меньше наименьшей» */
  bool fl_t25 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t24.as.number >= naimenshaya.as.number), "ширина не меньше наименьшей", "Ширина колонки", &fl_t25, error));
  if (!fl_t25) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «ширина не меньше наименьшей» функции «Ширина колонки»");
  }
  if (okno.tag != FL_NUMBER || naimenshaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, okno, naimenshaya, error));
  bool fl_t26 = false;
  FL_TRY(fl_cond(ctx, fl_flag(okno.as.number > naimenshaya.as.number), &fl_t26, error));
  fl_value fl_t27 = fl_nothing();
  if (fl_t26) {
    fl_t27 = okno;
  } else {
    fl_t27 = naimenshaya;
  }
  if (fl_t24.tag != FL_NUMBER || fl_t27.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t24, fl_t27, error));
  /* постусловие «ширина не больше окна, а если окно уже наименьшей — не больше наименьшей» */
  bool fl_t28 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t24.as.number <= fl_t27.as.number), "ширина не больше окна, а если окно уже наименьшей — не больше наименьшей", "Ширина колонки", &fl_t28, error));
  if (!fl_t28) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «ширина не больше окна, а если окно уже наименьшей — не больше наименьшей» функции «Ширина колонки»");
  }
  *result = fl_t24;
  return FL_OK;
}

/*
 * Функция flang «Ширина колонки по пресету».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param preset — «пресет»: число
 * @param zazor — «зазор»: число
 * @param naimenshaya — «наименьшая»: число
 * @param dolya0 — «доля0»: число
 * @param dolya1 — «доля1»: число
 * @param dolya2 — «доля2»: число
 * @param dolya3 — «доля3»: число
 * @return значение: число
 */
fl_status geometry_shirina_kolonki_po_presetu(fl_ctx *ctx, fl_value okno, fl_value preset, fl_value zazor, fl_value naimenshaya, fl_value dolya0, fl_value dolya1, fl_value dolya2, fl_value dolya3, fl_value *result, fl_error *error) {
  fl_value fl_t29 = fl_nothing();
  FL_TRY(geometry_preset_v_predelah(ctx, preset, &fl_t29, error));
  const fl_value nomer = fl_t29; /* пусть «номер» */
  bool fl_t30 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(nomer, fl_number(2.0))), &fl_t30, error));
  fl_value fl_t31 = fl_nothing();
  if (fl_t30) {
    fl_t31 = dolya2;
  } else {
    fl_t31 = dolya3;
  }
  const fl_value hvost = fl_t31; /* пусть «хвост» */
  bool fl_t32 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(nomer, fl_number(1.0))), &fl_t32, error));
  fl_value fl_t33 = fl_nothing();
  if (fl_t32) {
    fl_t33 = dolya1;
  } else {
    fl_t33 = hvost;
  }
  const fl_value seredina = fl_t33; /* пусть «середина» */
  bool fl_t34 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(nomer, fl_number(0.0))), &fl_t34, error));
  fl_value fl_t35 = fl_nothing();
  if (fl_t34) {
    fl_t35 = dolya0;
  } else {
    fl_t35 = seredina;
  }
  const fl_value dolya = fl_t35; /* пусть «доля» */
  fl_value fl_t36 = fl_nothing();
  FL_TRY(geometry_shirina_kolonki(ctx, okno, dolya, zazor, naimenshaya, &fl_t36, error));
  const fl_value fl_t37 = fl_t36;
  if (fl_t37.tag != FL_NUMBER || naimenshaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t37, naimenshaya, error));
  /* постусловие «ширина не меньше наименьшей» */
  bool fl_t38 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t37.as.number >= naimenshaya.as.number), "ширина не меньше наименьшей", "Ширина колонки по пресету", &fl_t38, error));
  if (!fl_t38) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «ширина не меньше наименьшей» функции «Ширина колонки по пресету»");
  }
  *result = fl_t37;
  return FL_OK;
}

/*
 * Функция flang «Высота окна».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param okon — «окон»: число
 * @param nomer — «номер»: число
 * @param zazor — «зазор»: число
 * @param naimenshaya — «наименьшая»: число
 * @return значение: число
 */
fl_status geometry_vysota_okna(fl_ctx *ctx, fl_value okno, fl_value okon, fl_value nomer, fl_value zazor, fl_value naimenshaya, fl_value *result, fl_error *error) {
  if (okno.tag != FL_NUMBER || naimenshaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, okno, naimenshaya, error));
  bool fl_t39 = false;
  FL_TRY(fl_cond(ctx, fl_flag(okno.as.number > naimenshaya.as.number), &fl_t39, error));
  fl_value fl_t40 = fl_nothing();
  if (fl_t39) {
    fl_t40 = okno;
  } else {
    fl_t40 = naimenshaya;
  }
  const fl_value predel = fl_t40; /* пусть «предел» */
  if (okon.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", okon, fl_number(1.0), error));
  if (zazor.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "mul", zazor, fl_number((okon.as.number - 1.0)), error));
  if (okno.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", okno, fl_number((zazor.as.number * (okon.as.number - 1.0))), error));
  const fl_value vsego = fl_number(okno.as.number - (zazor.as.number * (okon.as.number - 1.0))); /* пусть «всего» */
  fl_value fl_t41 = fl_nothing();
  FL_TRY(geometry_delenie_nacelo(ctx, vsego, okon, &fl_t41, error));
  const fl_value dolya = fl_t41; /* пусть «доля» */
  if (okon.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", okon, fl_number(1.0), error));
  bool fl_t42 = false;
  FL_TRY(fl_cond(ctx, fl_flag(fl_equal(nomer, fl_number(okon.as.number - 1.0))), &fl_t42, error));
  fl_value fl_t43 = fl_nothing();
  if (fl_t42) {
    if (okon.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", okon, fl_number(1.0), error));
    if (dolya.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "mul", dolya, fl_number((okon.as.number - 1.0)), error));
    if (vsego.tag != FL_NUMBER) FL_TRY(fl_not_numbers(ctx, "sub", vsego, fl_number((dolya.as.number * (okon.as.number - 1.0))), error));
    fl_t43 = fl_number(vsego.as.number - (dolya.as.number * (okon.as.number - 1.0)));
  } else {
    fl_t43 = dolya;
  }
  const fl_value syraya = fl_t43; /* пусть «сырая» */
  if (syraya.tag != FL_NUMBER || okno.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, syraya, okno, error));
  bool fl_t44 = false;
  FL_TRY(fl_cond(ctx, fl_flag(syraya.as.number > okno.as.number), &fl_t44, error));
  fl_value fl_t45 = fl_nothing();
  if (fl_t44) {
    fl_t45 = okno;
  } else {
    fl_t45 = syraya;
  }
  const fl_value sverhu = fl_t45; /* пусть «сверху» */
  if (sverhu.tag != FL_NUMBER || naimenshaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, sverhu, naimenshaya, error));
  bool fl_t46 = false;
  FL_TRY(fl_cond(ctx, fl_flag(sverhu.as.number < naimenshaya.as.number), &fl_t46, error));
  fl_value fl_t47 = fl_nothing();
  if (fl_t46) {
    fl_t47 = naimenshaya;
  } else {
    fl_t47 = sverhu;
  }
  const fl_value snizu = fl_t47; /* пусть «снизу» */
  if (okon.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, okon, fl_number(0.0), error));
  bool fl_t48 = false;
  FL_TRY(fl_cond(ctx, fl_flag(okon.as.number <= 0.0), &fl_t48, error));
  fl_value fl_t49 = fl_nothing();
  if (fl_t48) {
    fl_t49 = predel;
  } else {
    fl_t49 = snizu;
  }
  const fl_value fl_t50 = fl_t49;
  if (fl_t50.tag != FL_NUMBER || naimenshaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t50, naimenshaya, error));
  /* постусловие «высота не меньше наименьшей» */
  bool fl_t51 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t50.as.number >= naimenshaya.as.number), "высота не меньше наименьшей", "Высота окна", &fl_t51, error));
  if (!fl_t51) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «высота не меньше наименьшей» функции «Высота окна»");
  }
  if (okno.tag != FL_NUMBER || naimenshaya.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, okno, naimenshaya, error));
  bool fl_t52 = false;
  FL_TRY(fl_cond(ctx, fl_flag(okno.as.number > naimenshaya.as.number), &fl_t52, error));
  fl_value fl_t53 = fl_nothing();
  if (fl_t52) {
    fl_t53 = okno;
  } else {
    fl_t53 = naimenshaya;
  }
  if (fl_t50.tag != FL_NUMBER || fl_t53.tag != FL_NUMBER) FL_TRY(fl_not_order(ctx, fl_t50, fl_t53, error));
  /* постусловие «высота не больше окна, а если окно ниже наименьшей — не больше наименьшей» */
  bool fl_t54 = false;
  FL_TRY(fl_post(ctx, fl_flag(fl_t50.as.number <= fl_t53.as.number), "высота не больше окна, а если окно ниже наименьшей — не больше наименьшей", "Высота окна", &fl_t54, error));
  if (!fl_t54) {
    return fl_fail(ctx, error, "FLANG_PROPERTY", "%s", "нарушено свойство «высота не больше окна, а если окно ниже наименьшей — не больше наименьшей» функции «Высота окна»");
  }
  *result = fl_t50;
  return FL_OK;
}

/*
 * Вызов по исходному имени flang. Коды и тексты — те же, что у
 * интерпретатора: «не найдена функция …» и «функция … принимает N аргум.».
 */
fl_status geometry_call(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error) {
  if (strcmp(name, "Деление нацело") == 0) {
    if (count != 2) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Деление нацело", (unsigned long)2, (unsigned long)count);
    }
    return geometry_delenie_nacelo(ctx, args[0], args[1], result, error);
  }
  if (strcmp(name, "Пресет в пределах") == 0) {
    if (count != 1) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Пресет в пределах", (unsigned long)1, (unsigned long)count);
    }
    return geometry_preset_v_predelah(ctx, args[0], result, error);
  }
  if (strcmp(name, "Ширина колонки") == 0) {
    if (count != 4) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Ширина колонки", (unsigned long)4, (unsigned long)count);
    }
    return geometry_shirina_kolonki(ctx, args[0], args[1], args[2], args[3], result, error);
  }
  if (strcmp(name, "Ширина колонки по пресету") == 0) {
    if (count != 8) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Ширина колонки по пресету", (unsigned long)8, (unsigned long)count);
    }
    return geometry_shirina_kolonki_po_presetu(ctx, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], result, error);
  }
  if (strcmp(name, "Высота окна") == 0) {
    if (count != 5) {
      return fl_fail(ctx, error, FL_CODE_TYPE, "функция «%s» принимает %lu аргум., получено %lu",
                     "Высота окна", (unsigned long)5, (unsigned long)count);
    }
    return geometry_vysota_okna(ctx, args[0], args[1], args[2], args[3], args[4], result, error);
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
fl_status geometry_enter(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error) {
  FL_TRY(fl_check_entry(ctx, geometry_entry(), name, args, count, error));
  return geometry_call(ctx, name, args, count, result, error);
}

/*
 * Граница входа: объявленные типы параметров данными. Прогонщик сверяет по
 * ним значения, пришедшие снаружи, ДО вызова (fl_check_entry).
 *
 * Виды `неизвестно` (значение-функция, параметр полиморфизма, применение
 * типа с аргументами) не сверяются — ровно как молчит о них проверка
 * значений свидетеля.
 */
static const fl_type geometry_entry_types[] = {
  { FL_TYPE_NUMBER, "число", "", false, false, false, 0.0, 0.0, 0, 0, 0, 0, 0 },
};

static const fl_entry_param geometry_entry_params[] = {
  { "Деление нацело", "делимое", 0 },
  { "Деление нацело", "делитель", 0 },
  { "Пресет в пределах", "пресет", 0 },
  { "Ширина колонки", "окно", 0 },
  { "Ширина колонки", "доля", 0 },
  { "Ширина колонки", "зазор", 0 },
  { "Ширина колонки", "наименьшая", 0 },
  { "Ширина колонки по пресету", "окно", 0 },
  { "Ширина колонки по пресету", "пресет", 0 },
  { "Ширина колонки по пресету", "зазор", 0 },
  { "Ширина колонки по пресету", "наименьшая", 0 },
  { "Ширина колонки по пресету", "доля0", 0 },
  { "Ширина колонки по пресету", "доля1", 0 },
  { "Ширина колонки по пресету", "доля2", 0 },
  { "Ширина колонки по пресету", "доля3", 0 },
  { "Высота окна", "окно", 0 },
  { "Высота окна", "окон", 0 },
  { "Высота окна", "номер", 0 },
  { "Высота окна", "зазор", 0 },
  { "Высота окна", "наименьшая", 0 },
};

static const fl_entry_table geometry_entry_table = {
  geometry_entry_types, 1,
  NULL, 0,
  NULL, 0,
  geometry_entry_params, 20
};

const fl_entry_table *geometry_entry(void) {
  return &geometry_entry_table;
}
