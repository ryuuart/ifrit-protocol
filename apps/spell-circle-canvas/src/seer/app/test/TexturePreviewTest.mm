#import <Metal/Metal.h>

#include <gtest/gtest.h>
#include <sigilio/publish/Publisher.h>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtGui/QImage>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRendererInterface>
#include <functional>
#include <memory>
#include <vector>

#include "Application.h"
#include "TexturePreview.h"

namespace {

TEST(SeerTexturePreview, OwnsFramesAcrossPauseResizeReconnectAndWindowTeardown) {
  ensureSeerTestApplication();
  if (QGuiApplication::platformName() != "cocoa")
    GTEST_SKIP() << "native Qt window required; set QT_QPA_PLATFORM=cocoa";
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) GTEST_SKIP() << "no Metal device";
    id<MTLCommandQueue> queue = [device newCommandQueue];
    ASSERT_TRUE(queue);
    const std::string name = NSUUID.UUID.UUIDString.UTF8String;
    auto makePublisher = [&] {
      return sigil::io::publish::createPublisher(name, sigil::io::publish::Backend::Metal,
                                                 (__bridge void*)device);
    };
    auto publisher = makePublisher();
    ASSERT_TRUE(publisher);
    auto publish = [&](int width, int height, uint32_t pixel) {
      MTLTextureDescriptor* descriptor =
          [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                             width:width
                                                            height:height
                                                         mipmapped:NO];
      descriptor.storageMode = MTLStorageModeShared;
      id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
      std::vector<uint32_t> pixels(width * height, pixel);
      [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
                 mipmapLevel:0
                   withBytes:pixels.data()
                 bytesPerRow:width * 4];
      id<MTLCommandBuffer> commands = [queue commandBuffer];
      publisher->publishFrame((__bridge void*)texture, (__bridge void*)commands, width, height);
      [commands commit];
      [commands waitUntilCompleted];
      EXPECT_EQ(commands.status, MTLCommandBufferStatusCompleted);
    };
    publish(32, 24, 0xff3366cc);

    {
      QQuickWindow window;
      window.resize(240, 180);
      auto* preview = new TexturePreview(window.contentItem());
      preview->setWidth(240);
      preview->setHeight(180);
      preview->setSource(QString::fromStdString(name));
      window.show();
      auto pump = [&](const std::function<bool()>& done) {
        QElapsedTimer elapsed;
        elapsed.start();
        do {
          QCoreApplication::processEvents();
          window.grabWindow();
          QCoreApplication::processEvents();
          if (done()) return true;
          QThread::msleep(5);
        } while (elapsed.elapsed() < 4000);
        return false;
      };
      ASSERT_TRUE(
          pump([&] { return preview->connected() && preview->frameSize() == QSize(32, 24); }));
      ASSERT_EQ(window.rendererInterface()->graphicsApi(), QSGRendererInterface::Metal);
      EXPECT_EQ(window.grabWindow().pixelColor(120, 90), QColor(0x33, 0x66, 0xcc));

      preview->setPaused(true);
      publish(48, 20, 0xffcc6633);
      for (int i = 0; i < 6; ++i) {
        QCoreApplication::processEvents();
        window.grabWindow();
      }
      EXPECT_EQ(preview->frameSize(), QSize(32, 24));
      EXPECT_EQ(window.grabWindow().pixelColor(120, 90), QColor(0x33, 0x66, 0xcc));
      preview->setPaused(false);
      ASSERT_TRUE(pump([&] { return preview->frameSize() == QSize(48, 20); }));
      EXPECT_EQ(window.grabWindow().pixelColor(120, 90), QColor(0xcc, 0x66, 0x33));

      QTemporaryDir capture;
      bool saved = false;
      QObject::connect(preview, &TexturePreview::saved, preview,
                       [&](const QString&, bool success) { saved = success; });
      preview->saveFrame(QUrl::fromLocalFile(capture.filePath("frame.png")));
      ASSERT_TRUE(pump([&] { return saved; }));
      const QImage captured(capture.filePath("frame.png"));
      ASSERT_EQ(captured.size(), QSize(48, 20));
      EXPECT_EQ(captured.pixelColor(0, 0), QColor(0xcc, 0x66, 0x33));

      publisher.reset();
      ASSERT_TRUE(pump([&] { return !preview->connected(); }));
      EXPECT_EQ(preview->frameSize(), QSize(48, 20));
      publisher = makePublisher();
      ASSERT_TRUE(publisher);
      publish(64, 40, 0xff339966);
      ASSERT_TRUE(
          pump([&] { return preview->connected() && preview->frameSize() == QSize(64, 40); }));
      EXPECT_EQ(window.grabWindow().pixelColor(120, 90), QColor(0x33, 0x99, 0x66));

      preview->setSource(QString::fromStdString(name) + "-waiting");
      ASSERT_TRUE(pump([&] { return !preview->connected() && preview->frameSize().isEmpty(); }));
      preview->setSource(QString::fromStdString(name));
      ASSERT_TRUE(
          pump([&] { return preview->connected() && preview->frameSize() == QSize(64, 40); }));
      window.close();
    }
    QCoreApplication::processEvents();
  }
}

}  // namespace
