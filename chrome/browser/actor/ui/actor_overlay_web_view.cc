// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_web_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/view_class_properties.h"

#include "custom_browser/buildflags.h"

ActorOverlayWebView::ActorOverlayWebView(BrowserWindowInterface* browser)
    : browser_(browser) {
  // Required to create a new web contents if one doesn't exist.
  SetBrowserContext(browser->GetProfile());
  SetVisible(false);
  SetFocusBehavior(views::View::FocusBehavior::NEVER);
  GetViewAccessibility().SetIsIgnored(true);
}

ActorOverlayWebView::~ActorOverlayWebView() {
  CloseUI();
  SetWebContents(nullptr);
}

void ActorOverlayWebView::ShowUI(tabs::TabInterface* tab,
                                 base::OnceClosure callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActorUi));
  CHECK(features::kGlicActorUiOverlay.Get());
  if (!web_contents()) {
    // Creates a new web contents if one doesn't exist.
    LoadInitialURL(GURL(chrome::kChromeUIActorOverlayURL));
  }
  // Disable mouse, keyboard, and a11y input events to underlying tab
  // contents.
  scoped_ignore_input_events_ =
      tab->GetContents()->IgnoreInputEvents(std::nullopt);
  // Set the tab interface
  webui::SetTabInterface(web_contents(), tab);

  // Make the view background transparent so it can act as an overlay.
  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (rwhv) {
    rwhv->SetBackgroundColor(SK_ColorTRANSPARENT);
  }

  SetVisible(true);
  web_contents()->WasShown();
  // If the WebUI is available, let it handle the callback. Otherwise, add the
  // callback to the list until the navigation commits.
  if (actor::ui::ActorOverlayUI* webui = GetWebUi()) {
    webui->SetHandlerInitializedCallback(std::move(callback));
  } else {
    pending_webui_init_callbacks_.push_back(std::move(callback));
  }
}

void ActorOverlayWebView::CloseUI() {
  if (web_contents()) {
    SetVisible(false);
    // Re-enable mouse, keyboard, and a11y input events to the underlying web
    // contents by resetting the ScopedIgnoreInputEvents object.
    scoped_ignore_input_events_.reset();
    web_contents()->WasHidden();
  }
}

void ActorOverlayWebView::SetOverlayBackground(bool is_visible) {
  actor::ui::ActorOverlayUI* web_ui = GetWebUi();
  if (!web_ui) {
    return;
  }

  web_ui->SetOverlayBackground(is_visible);
}

void ActorOverlayWebView::SetBorderGlowVisibility(bool is_visible) {
  actor::ui::ActorOverlayUI* web_ui = GetWebUi();
  if (!web_ui) {
    return;
  }

  web_ui->SetBorderGlowVisibility(is_visible);
}

void ActorOverlayWebView::MoveCursorTo(const gfx::Point& point,
                                       base::OnceClosure callback) {
  // Ensure the callback runs on any early return. We Release() ownership only
  // when passing the callback to an asynchronous operation.
  base::ScopedClosureRunner runner(std::move(callback));
  if (!base::FeatureList::IsEnabled(features::kGlicActorUiMagicCursor)) {
    return;
  }
#if BUILDFLAG(ENABLE_CUSTOM_BROWSER)
  // custom-browser: do not gate the action on an animation nobody can see.
  // The page resolves the move on the cursor's CSS `transitionend` and the
  // click on `animationend`, and nothing else resolves them
  // (crbug.com/454339982). In the kNexus window this overlay sits in the
  // contents container, and while no guest shows the agent's tab (a
  // background chat session, or a session driven only by a scoped runtime
  // connection such as a remote client) that container is either hidden by
  // the layout or, if no <main-contents-view> layout ever arrived, still at
  // its initial 0x0. Either way the overlay has no visible bounds, the same
  // test NativeViewHost uses to hide the native view, and an overlay page
  // with no pixels on screen never finishes the animation. ExecutionEngine
  // then never leaves kUiPreInvoke and the click / type waits out the whole
  // tool budget before it is dispatched. ShowUI() forces the overlay
  // WebContents visible (WasShown()), so its GetVisibility() cannot tell.
  // Returning here lets `runner` complete the step. Chromium enables the
  // magic cursor only through the fieldtrial testing config, so this shows
  // in Debug builds (and the e2e suite), not in an official build.
  if (GetVisibleBounds().IsEmpty()) {
    return;
  }
#endif
  if (actor::ui::ActorOverlayUI* web_ui = GetWebUi()) {
    web_ui->MoveCursorTo(point, runner.Release());
  }
}

void ActorOverlayWebView::TriggerClickAnimation(base::OnceClosure callback) {
  // Ensure the callback runs on any early return. We Release() ownership only
  // when passing the callback to an asynchronous operation.
  base::ScopedClosureRunner runner(std::move(callback));
  if (!base::FeatureList::IsEnabled(features::kGlicActorUiMagicCursor)) {
    return;
  }
#if BUILDFLAG(ENABLE_CUSTOM_BROWSER)
  // custom-browser: same reason as MoveCursorTo; an overlay with no visible
  // bounds never fires `animationend`.
  if (GetVisibleBounds().IsEmpty()) {
    return;
  }
#endif
  if (actor::ui::ActorOverlayUI* web_ui = GetWebUi()) {
    web_ui->TriggerClickAnimation(runner.Release());
  }
}

actor::ui::ActorOverlayUI* ActorOverlayWebView::GetWebUi() {
  if (!web_contents()) {
    return nullptr;
  }

  return web_contents()
      ->GetWebUI()
      ->GetController()
      ->GetAs<actor::ui::ActorOverlayUI>();
}

void ActorOverlayWebView::PrimaryPageChanged(content::Page& page) {
  if (pending_webui_init_callbacks_.empty()) {
    return;
  }
  if (actor::ui::ActorOverlayUI* webui = GetWebUi()) {
    for (auto& callback : pending_webui_init_callbacks_) {
      webui->SetHandlerInitializedCallback(std::move(callback));
    }
  } else {
    // TODO(crbug.com/422539773): Handle when WebUI initialization fails, this
    // could happen if the navigation fails or renderer crashes. Running
    // callback to not block actuation.
    for (auto& callback : pending_webui_init_callbacks_) {
      std::move(callback).Run();
    }
  }
  pending_webui_init_callbacks_.clear();
}

BEGIN_METADATA(ActorOverlayWebView)
END_METADATA
