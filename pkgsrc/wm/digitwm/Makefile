# $NetBSD$

DISTNAME=	digitwm-0.1.0
CATEGORIES=	wm x11
MASTER_SITES=	${MASTER_SITE_GITHUB:=digitable-lol/}
GITHUB_PROJECT=	digitwm
GITHUB_TAG=	v${PKGVERSION_NOREV}

MAINTAINER=	noreply@digitable.life
HOMEPAGE=	https://github.com/digitable-lol/digitwm/
COMMENT=	Scrollable-tiling window manager for X11, forked from cwm
LICENSE=	isc

USE_LANGUAGES=	c
USE_TOOLS+=	pkg-config yacc

# Upstream keeps the binary of the fork named cwm, so that a cwmrc and the
# manual pages carry over unchanged.  The price is that the two cannot be
# installed side by side.
CONFLICTS+=	cwm-[0-9]*

MAKE_FLAGS+=		PREFIX=${PREFIX}
INSTALL_MAKE_FLAGS+=	PREFIX=${PREFIX} MANPREFIX=${PREFIX}/${PKGMANDIR}

# The window manager itself needs nothing but these three libraries, a C
# compiler and yacc.  The FTS models under fts/ are checked in CI and are not
# built, installed or needed at runtime - that is what keeps this package
# free of a Node dependency on NetBSD.
.include "../../x11/libX11/buildlink3.mk"
.include "../../x11/libXft/buildlink3.mk"
.include "../../x11/libXrandr/buildlink3.mk"
.include "../../mk/bsd.pkg.mk"
