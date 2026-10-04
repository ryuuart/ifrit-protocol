#pragma once

#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Stroke.h>
#include <sigildraw/brush/Tool.h>
#include <sigildraw/brush/Wash.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>

#include <array>

namespace painted_fields {

inline constexpr int kFieldWidth = 1024;
inline constexpr int kFieldHeight = 416;

inline void paintHeight(sigil::draw::Pen& pen, int phase) {
  namespace draw = sigil::draw;
  namespace brush = sigil::draw::brush;
  namespace compose = sigil::compose;

  pen.push();
  pen.colorMode(draw::RGB);
  pen.blendMode(draw::BLEND);
  pen.angleMode(draw::RADIANS);
  pen.rectMode(draw::CORNER);
  pen.ellipseMode(draw::CENTER);
  pen.smooth();
  pen.noDash();
  pen.background(0);
  if (phase == 7) {
    pen.pop();
    return;
  }
  pen.randomSeed(0xF108u);
  pen.clip([&] { pen.rect(0, 0, kFieldWidth, kFieldHeight); });

  const std::array<SkPoint, 9> washBoundary{{
      {54, 320},
      {248, 287},
      {430, 333},
      {628, 304},
      {802, 317},
      {978, 378},
      {712, 399},
      {458, 381},
      {201, 393},
  }};
  brush::wash(pen,
              {.color = {.36f, .36f, .36f, 1},
               .opacity = .54f,
               .bleed = .045f,
               .texture = .72f,
               .border = .26f,
               .layers = 8,
               .blend = draw::BLEND},
              washBoundary);

  auto bristles = brush::watercolor({.74f, .74f, .74f, 1}, 68);
  bristles.blend = draw::BLEND;
  bristles.spacing = 2.4f;
  bristles.opacity = .68f;
  bristles.scatter = 1.8f;
  bristles.density = .83f;
  bristles.bristles = 38;
  bristles.pressure = {.22f, .98f, .14f};
  const std::array<brush::Sample, 5> lowerSweep{{
      {{30, 320}, .42f},
      {{245, 279}, .92f},
      {{505, 355}, .78f},
      {{780, 294}, 1.0f},
      {{996, 341}, .28f},
  }};
  brush::spline(pen, bristles, lowerSweep, .8f);

  bristles.color = {.92f, .92f, .92f, 1};
  bristles.width = 42;
  bristles.bristles = 29;
  bristles.opacity = .75f;
  bristles.pressure = {.16f, .86f, .25f};
  const std::array<brush::Sample, 5> crossingSweep{{
      {{34, 388}, .30f},
      {{278, 350}, .90f},
      {{522, 312}, 1.0f},
      {{750, 378}, .75f},
      {{998, 371}, .22f},
  }};
  brush::spline(pen, bristles, crossingSweep, .78f);

  auto filament = brush::pencil({.66f, .66f, .66f, 1}, 1.15f);
  filament.blend = draw::BLEND;
  filament.opacity = .56f;
  filament.scatter = .2f;
  filament.spacing = 1.25f;
  for (int i = 0; i < 18; ++i) {
    const float lane = static_cast<float>(i);
    const float offset = pen.random(-2.5f, 2.5f);
    const std::array<brush::Sample, 5> strand{{
        {{18, 337 + lane * 2.5f}, .12f},
        {{251, 311 + lane * 1.7f + offset}, .82f},
        {{510, 374 - lane * .8f}, .68f},
        {{781, 327 + lane * 1.1f + offset}, .90f},
        {{1001, 391 - lane * .9f}, .08f},
    }};
    brush::spline(pen, filament, strand, .82f);
  }

  pen.element(
      compose::box()
          .width(940)
          .height(210)
          .alignItems(compose::Align::Center)
          .justifyContent(compose::Justify::Center)
          .children({compose::text("FIELD 08")
                         .fontFamily("Georgia, serif")
                         .fontSize(170)
                         .fontWeight(700)
                         .ink(sigil::material::Color{.96f, .96f, .96f, 1})
                         .filter(sigil::material::Filter::blur(1.2f))}),
      42, 72, 940, 210);

  if (phase == 6) {
    bristles.width = 58;
    bristles.color = {.83f, .83f, .83f, 1};
    const std::array<brush::Sample, 5> boundarySweep{{
        {{-96, 304}, .75f},
        {{208, 369}, 1.0f},
        {{489, 345}, .90f},
        {{797, 391}, 1.0f},
        {{1128, 334}, .85f},
    }};
    brush::spline(pen, bristles, boundarySweep, .8f);
    pen.noStroke();
    pen.fill(0);
    pen.push();
    pen.translate(514, 208);
    pen.rotate(-.22f);
    pen.rect(-19, -250, 38, 500);
    pen.pop();
    pen.circle(804, 310, 104);
  }
  pen.pop();
}

inline void paintWetness(sigil::draw::Pen& pen, int phase) {
  namespace draw = sigil::draw;
  namespace brush = sigil::draw::brush;

  pen.push();
  pen.colorMode(draw::RGB);
  pen.blendMode(draw::BLEND);
  pen.angleMode(draw::RADIANS);
  pen.rectMode(draw::CORNER);
  pen.ellipseMode(draw::CENTER);
  pen.smooth();
  pen.noDash();
  pen.background(0);
  if (phase == 7) {
    pen.pop();
    return;
  }
  pen.randomSeed(0xA71Eu);
  pen.clip([&] { pen.rect(0, 0, kFieldWidth, kFieldHeight); });

  const std::array<SkPoint, 8> washBoundary{{
      {99, 333},
      {317, 198},
      {548, 69},
      {867, 37},
      {944, 117},
      {687, 256},
      {409, 366},
      {178, 391},
  }};
  brush::wash(pen,
              {.color = {.84f, .84f, .84f, 1},
               .opacity = .68f,
               .bleed = .12f,
               .texture = .66f,
               .border = .46f,
               .bleedAngle = -.52f,
               .layers = 10,
               .blend = draw::BLEND},
              washBoundary);

  auto loaded = brush::watercolor({.94f, .94f, .94f, 1}, 83);
  loaded.blend = draw::BLEND;
  loaded.spacing = 3;
  loaded.opacity = .58f;
  loaded.scatter = 3.5f;
  loaded.density = .91f;
  loaded.bristles = 43;
  loaded.pressure = {.23f, .98f, .18f};
  for (int i = 0; i < 4; ++i) {
    const float lane = static_cast<float>(i);
    const float offset = pen.random(-6, 6);
    const std::array<brush::Sample, 5> slantedPass{{
        {{106 + lane * 45, 358 + lane * 8}, .25f},
        {{285 + lane * 42, 275 + offset}, .87f},
        {{470 + lane * 44, 192 + offset}, 1.0f},
        {{696 + lane * 34, 99 + offset}, .88f},
        {{899 + lane * 16, 48 + lane * 15}, .22f},
    }};
    brush::spline(pen, loaded, slantedPass, .72f);
  }

  auto dryEdge = brush::pencil({.69f, .69f, .69f, 1}, 1.5f);
  dryEdge.blend = draw::BLEND;
  dryEdge.opacity = .48f;
  dryEdge.spacing = 1.6f;
  for (int i = 0; i < 14; ++i) {
    const float lane = static_cast<float>(i);
    const std::array<brush::Sample, 4> streak{{
        {{119, 329 + lane * 4}, .08f},
        {{332, 219 + lane * 3}, .72f},
        {{612, 121 + lane * 2.1f}, .88f},
        {{918, 45 + lane * 4.1f}, .06f},
    }};
    brush::spline(pen, dryEdge, streak, .73f);
  }

  pen.noStroke();
  pen.fill(255);
  pen.circle(944, 344, 28);

  if (phase == 6) {
    loaded.width = 104;
    const std::array<brush::Sample, 4> boundaryPass{{
        {{-94, 454}, .75f},
        {{254, 279}, 1.0f},
        {{727, 117}, 1.0f},
        {{1132, -54}, .85f},
    }};
    brush::spline(pen, loaded, boundaryPass, .7f);
    pen.noStroke();
    pen.fill(0);
    pen.push();
    pen.translate(531, 218);
    pen.rotate(-.22f);
    pen.rect(-19, -250, 38, 500);
    pen.pop();
    pen.circle(737, 157, 112);
  }
  pen.pop();
}

}  // namespace painted_fields
