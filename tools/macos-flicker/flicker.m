/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * digitwm - помощник, который честно рассказывает, когда его окно появилось и
 * когда его сдвинули
 *
 * Copyright (c) 2026 Digitable <https://digitable.life>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Мелькание при вставке - это время, которое окно стоит не на своём месте,
 * будучи уже видимым.  Снаружи его не измерить: снимок экрана скажет, что
 * нарисовано, но не когда.  Поэтому меряет само приложение - ровно так же, как
 * на X11 это делает tools/redraw-probe.c.
 *
 * Помощник открывает окна там, где их поставило бы обычное приложение (в
 * стороне от ленты), и печатает по строке на событие часами CLOCK_MONOTONIC_RAW
 * - теми же, по которым печатает axcost.c, так что две половины складываются
 * без поправок:
 *
 *   open  <мс> #k <x> <y>   - окно велено показать
 *   paint <мс> #k           - приложение нарисовало первый кадр
 *   moved <мс> #k <x> <y>   - окно уехало (это сделал менеджер, чужой процесс)
 *
 * Мелькание одного окна = moved - paint, плюс до одного кадра экрана на каждом
 * конце: сам компоузер показывает содержимое не раньше следующего обновления.
 * Частота экрана печатается в шапке, чтобы поправку было чем считать.
 *
 * Запуск (два терминала):
 *   ./axcost watch <pid помощника> 40      # менеджер: ждёт окна и двигает их
 *   ./flicker 8 1500                       # помощник: восемь окон по одному
 *
 * Оно НИ РАЗУ НЕ СОБИРАЛОСЬ: мака у нас нет.  Против заглушек проверена
 * только сишная половина (axcost.c, stub-build.sh); этот файл - нет, потому
 * что заглушки для AppKit честнее не делать вовсе.
 */

#import <Cocoa/Cocoa.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double
now_ms(void)
{
	struct timespec	 ts;

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0);
}

/* Вид, который сообщает о своём первом кадре и больше ничем не занят. */
@interface DgtPaintView : NSView
@property (assign) int idx;
@property (assign) BOOL painted;
@property (assign) double paintedAt;
@end

@implementation DgtPaintView

- (void)drawRect:(NSRect)dirty
{
	[[NSColor selectedControlColor] setFill];
	NSRectFill(dirty);

	if (!self.painted) {
		self.painted = YES;
		self.paintedAt = now_ms();
		printf("paint %.3f #%d\n", self.paintedAt, self.idx);
		fflush(stdout);
	}
}

@end

@interface DgtHelper : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSMutableArray<NSWindow *> *windows;
@property (strong) NSMutableArray<NSNumber *> *paintTimes;
@property (strong) NSMutableArray<NSNumber *> *moveTimes;
@property (assign) int wanted;
@property (assign) double interval;
@property (assign) int opened;
@end

@implementation DgtHelper

- (instancetype)initWithCount:(int)count interval:(double)seconds
{
	self = [super init];
	if (self != nil) {
		_windows = [NSMutableArray array];
		_paintTimes = [NSMutableArray array];
		_moveTimes = [NSMutableArray array];
		_wanted = count;
		_interval = seconds;
		_opened = 0;
	}
	return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
	NSScreen	*screen = [NSScreen mainScreen];

	printf("# помощник digitwm: %d окон, каждые %.0f мс\n", self.wanted,
	    self.interval * 1000.0);
	printf("# экран %.0fx%.0f, кадров в секунду не менее %ld\n",
	    screen.frame.size.width, screen.frame.size.height,
	    (long)screen.maximumFramesPerSecond);
	printf("# один кадр = %.1f мс - столько компоузер может добавить с "
	    "каждого конца\n",
	    1000.0 / (double)MAX(1, screen.maximumFramesPerSecond));
	fflush(stdout);

	[NSTimer scheduledTimerWithTimeInterval:self.interval
					 target:self
				       selector:@selector(openOne:)
				       userInfo:nil
					repeats:YES];
}

- (void)openOne:(NSTimer *)timer
{
	NSRect		 frame;
	NSWindow	*win;
	DgtPaintView	*view;
	int		 k;

	if (self.opened >= self.wanted) {
		[timer invalidate];
		[self performSelector:@selector(finish)
			   withObject:nil
			   afterDelay:1.5];
		return;
	}

	k = self.opened + 1;

	/*
	 * Место выбирает приложение, и оно нарочно не то: лента поставила бы
	 * окно в свою колонку.  Ступенька в 24 точки - чтобы окна не легли
	 * ровно друг на друга и система не двинула их сама.
	 */
	frame = NSMakeRect(120.0 + 24.0 * k, 160.0 + 24.0 * k, 480.0, 320.0);

	win = [[NSWindow alloc]
	    initWithContentRect:frame
		      styleMask:(NSWindowStyleMaskTitled |
				 NSWindowStyleMaskClosable |
				 NSWindowStyleMaskResizable)
			backing:NSBackingStoreBuffered
			  defer:NO];
	[win setTitle:[NSString stringWithFormat:@"digitwm probe %d", k]];
	[win setReleasedWhenClosed:NO];
	[win setDelegate:self];

	view = [[DgtPaintView alloc] initWithFrame:[[win contentView] bounds]];
	view.idx = k;
	[win setContentView:view];

	[self.windows addObject:win];
	[self.paintTimes addObject:@(0.0)];
	[self.moveTimes addObject:@(0.0)];
	self.opened = k;

	printf("open %.3f #%d %.0f %.0f\n", now_ms(), k, frame.origin.x,
	    frame.origin.y);
	fflush(stdout);

	[win makeKeyAndOrderFront:nil];
}

- (void)windowDidMove:(NSNotification *)note
{
	NSWindow	*win = (NSWindow *)[note object];
	NSUInteger	 at = [self.windows indexOfObject:win];
	double		 t = now_ms();

	if (at == NSNotFound)
		return;
	/* Только первый переезд: дальше окно двигает уже раскладка, не вставка. */
	if ([self.moveTimes[at] doubleValue] > 0.0)
		return;

	self.moveTimes[at] = @(t);
	printf("moved %.3f #%d %.0f %.0f\n", t, (int)at + 1,
	    win.frame.origin.x, win.frame.origin.y);
	fflush(stdout);
}

- (void)finish
{
	NSUInteger	 i;
	double		 sum = 0.0;
	int		 counted = 0;

	printf("\n# мелькание по окнам: от своего первого кадра до переезда\n");
	for (i = 0; i < [self.windows count]; i++) {
		DgtPaintView	*view = (DgtPaintView *)
		    [self.windows[i] contentView];
		double		 painted = view.painted ? view.paintedAt : 0.0;
		double		 moved = [self.moveTimes[i] doubleValue];

		if (painted <= 0.0 || moved <= 0.0) {
			printf("#%lu: не сдвинуто (менеджер не работал?)\n",
			    (unsigned long)i + 1);
			continue;
		}
		printf("#%lu: %.1f мс\n", (unsigned long)i + 1,
		    moved - painted);
		sum += moved - painted;
		counted++;
	}
	if (counted > 0)
		printf("# среднее по %d окнам: %.1f мс (плюс до кадра экрана "
		    "с каждого конца)\n", counted, sum / counted);
	else
		printf("# считать нечего: ни одно окно не переехало\n");
	fflush(stdout);

	[NSApp terminate:nil];
}

@end

int
main(int argc, const char *argv[])
{
	@autoreleasepool {
		int		 count = (argc > 1) ? atoi(argv[1]) : 8;
		double		 interval = ((argc > 2) ? atof(argv[2]) : 1500.0)
				     / 1000.0;
		NSApplication	*app = [NSApplication sharedApplication];
		DgtHelper	*helper;

		if (count < 1)
			count = 1;
		if (interval < 0.2)
			interval = 0.2;

		[app setActivationPolicy:NSApplicationActivationPolicyRegular];
		helper = [[DgtHelper alloc] initWithCount:count
						 interval:interval];
		[app setDelegate:helper];
		[app activateIgnoringOtherApps:YES];
		[app run];
	}
	return 0;
}
