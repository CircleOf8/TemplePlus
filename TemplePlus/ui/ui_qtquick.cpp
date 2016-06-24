
#include "stdafx.h"

#include <QtCore/QCoreApplication>
#include <QtQuick/QQuickRenderControl>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickItem>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOffscreenSurface>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "ui_qtquick.h"
#include "ui.h"

#include <graphics/device.h>
#include <graphics/dynamictexture.h>
#include <graphics/shaperenderer2d.h>
#include <tig/tig_startup.h>

/**
* Additional state associated with a legacy window.
*/
struct LgcyWidgetState : QObject {
Q_OBJECT
Q_DISABLE_COPY(LgcyWidgetState);
public:
	LgcyWidgetState(QOpenGLContext *glContext, QQmlEngine *engine, const std::string &path, int width, int height);

	std::string path;
	QQuickRenderControl renderControl;
	std::unique_ptr<QOpenGLFramebufferObject> fbo;
	QQuickWindow window;
	QQmlComponent component;
	QQuickItem *rootItem;
	gfx::RenderTargetTexturePtr rt;

public slots:
	void createFbo();
	void destroyFbo();
	void requestUpdate();
};

#include "ui_qtquick_moc.h"

static UiQtQuick *uiQtQuick = nullptr;

using LgcyWidgetStatePtr = std::unique_ptr<LgcyWidgetState>;

struct UiQtQuick::Impl {
	QOpenGLContext glContext;
	QOffscreenSurface offscreenSurface;
	QQmlEngine qmlEngine;

	eastl::hash_map<LgcyWidgetId, LgcyWidgetStatePtr> state;

	static void Render(LgcyWidgetId widgetId);
	static BOOL HandleMessage(LgcyWidgetId id, TigMsg *msg);
};

UiQtQuick::UiQtQuick() : mImpl(std::make_unique<Impl>())
{
	if (uiQtQuick == nullptr) {
		uiQtQuick = this;
	}

	mImpl->qmlEngine.setOutputWarningsToStandardError(true);

	mImpl->qmlEngine.addImportPath("D:\\Qt\\5.7\\msvc2015\\qml");
		
	QSurfaceFormat desiredFormat = QSurfaceFormat::defaultFormat();
	desiredFormat.setAlphaBufferSize(8);
	desiredFormat.setDepthBufferSize(16);
	desiredFormat.setStencilBufferSize(8);
	mImpl->glContext.setFormat(desiredFormat);
	if (!mImpl->glContext.create()) {
		throw TempleException("Unable to create the OpenGL context for offscreen UI rendering.");
	}

	// Use the concrete format used by the context
	mImpl->offscreenSurface.setFormat(mImpl->glContext.format());
	mImpl->offscreenSurface.create();
}

UiQtQuick::~UiQtQuick()
{
	if (uiQtQuick == this) {
		uiQtQuick = nullptr;
	}
}

LgcyWidgetId UiQtQuick::LoadWindow(int x, int y, int width, int height, const std::string & path)
{
	LgcyWindow window(x, y, width, height);
	window.render = Impl::Render;
	window.handleMessage = Impl::HandleMessage;
	
	if (ui.AddWindow(&window, sizeof(window), &window.widgetId, "", 0)) {
		throw TempleException("Unable to add window for QML File {}", path);
	}

	if (!mImpl->glContext.makeCurrent(&mImpl->offscreenSurface)) {
		throw TempleException("Unable to make the offscreen surface current.");
	}

	// Create state / load QML file
	auto state = std::make_unique<LgcyWidgetState>(
		&mImpl->glContext,
		&mImpl->qmlEngine,
		path,
		width,
		height
	);
	
	mImpl->state[window.widgetId] = std::move(state);
	
	QCoreApplication::processEvents();
		
	return window.widgetId;
}

void UiQtQuick::Impl::Render(LgcyWidgetId widgetId)
{
	auto self = uiQtQuick->mImpl.get();

	auto it = self->state.find(widgetId);
	if (it == self->state.end()) {
		return;
	}

	auto &state = *it->second;

	self->glContext.makeCurrent(&self->offscreenSurface);

	QCoreApplication::processEvents();
	
	state.renderControl.polishItems();
	state.renderControl.sync();
	state.renderControl.render();
	self->glContext.functions()->glFlush();

	if (state.rt) {
		auto widget = ui.WidgetGet(widgetId);
		auto rtSize = state.rt->GetSize();
		tig->GetShapeRenderer2d().DrawRectangle(
			widget->x,
			widget->y,
			rtSize.width,
			rtSize.height,
			*state.rt
		);
	}

}

BOOL UiQtQuick::Impl::HandleMessage(LgcyWidgetId id, TigMsg * msg)
{
	return FALSE;
}

LgcyWidgetState::LgcyWidgetState(QOpenGLContext *glContext,
	QQmlEngine *engine,
	const std::string &path,
	int width,
	int height) 
	: path(path), window(&renderControl), component(engine, QString::fromStdString(path), QQmlComponent::PreferSynchronous)
{
	// Connect to the renderer signals
	// Now hook up the signals. For simplicy we don't differentiate between
	// renderRequested (only render is needed, no sync) and sceneChanged (polish and sync
	// is needed too).
	connect(&window, SIGNAL(sceneGraphInitialized()), this, SLOT(createFbo()));
	connect(&window, SIGNAL(sceneGraphInvalidated()), this, SLOT(destroyFbo()));
	connect(&renderControl, SIGNAL(renderRequested()), this, SLOT(requestUpdate()));
	connect(&renderControl, SIGNAL(sceneChanged()), this, SLOT(requestUpdate()));
		
	auto rootObject = component.create();
	if (!rootObject) {
		throw TempleException("Creation of QML object has failed: {}", component.errorString().toStdString());
	}

	// NOTE: Due to delay loading constraints, we can't use qobject_cast here
	rootItem = (QQuickItem *)(rootObject);
	if (!rootItem) {
		throw TempleException("Root of QML file '{}' is not a QQuickItem", path);
	}

	// Put the root item into the window
	rootItem->setParentItem(window.contentItem());

	// Size the item / window to the requested size
	rootItem->setWidth(width);
	rootItem->setHeight(height);
	window.setGeometry(0, 0, width, height);
	
	renderControl.initialize(glContext);
}

void LgcyWidgetState::createFbo()
{
	// The scene graph has been initialized. It is now time to create an FBO and associate
	// it with the QQuickWindow.
	fbo.reset(new QOpenGLFramebufferObject(window.size(), QOpenGLFramebufferObject::CombinedDepthStencil));
	window.setRenderTarget(fbo.get());

	auto texId = fbo->texture();
	glBindTexture(GL_TEXTURE_2D, texId);

	auto width = window.size().width();
	auto height = window.size().height();
	
	using namespace gfx;
	rt = renderingDevice->CreateRenderTargetTexture(BufferFormat::A8R8G8B8, width, height, false, true);

	Qt::HANDLE sharedHandle = rt->GetShareHandle();
	
	// Create a EGL native surface from the share handle
	EGLint bufferAttributes[] = {
		EGL_WIDTH, width,
		EGL_HEIGHT, height,
		EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
		EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA, // TODO: check correctness of this
		EGL_NONE
	};

	EGLint config_attribs[] = { 
		EGL_BUFFER_SIZE,  32,
		EGL_RED_SIZE,     8,
		EGL_GREEN_SIZE,   8,
		EGL_BLUE_SIZE,    8,
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_ALPHA_SIZE,   8,
		EGL_NONE
	};

	auto display = eglGetCurrentDisplay();

	EGLConfig eglConfig;
	EGLint numConfigs;
	if (!eglChooseConfig(display, config_attribs, &eglConfig, 1, &numConfigs)) {
		throw TempleException("Unable to create EGL config.");
	}
		
	auto surface = eglCreatePbufferFromClientBuffer(display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, sharedHandle, eglConfig, bufferAttributes);
	if (surface == EGL_NO_SURFACE) {
		throw new TempleException("Unable to open shared handle.");
	}

	glBindTexture(GL_TEXTURE_2D, fbo->texture());
	eglBindTexImage(display, surface, EGL_BACK_BUFFER);

}

void LgcyWidgetState::destroyFbo()
{
	fbo.reset();
}

void LgcyWidgetState::requestUpdate()
{
}
