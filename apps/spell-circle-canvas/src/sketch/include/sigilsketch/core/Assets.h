#pragma once

/** @file
 * @ingroup sketch-core
 *
 * The resource hub a host mounts for its sketches, and the probe a
 * sketch over fetched art answers its availability with.
 */

#include <sigilio/hub/Hub.h>

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

/** THE FILES A SKETCH REACHES FOR that it did not generate.
 *
 *  The demo assets root mounts at `res://` and the sketches folder at
 *  `sketch://`, under which a sketch's own files stand. `hub()` is the
 *  one resource entrance, with SigilMedia's and SigilData's decoders and
 *  SigilIO's transports on it, answering a sketch compiled into the host
 *  and one compiled and loaded while it runs alike:
 *  `hub().load<media::Image>("res://ui/mark.png")`,
 *  `hub().load<data::Table>("res://cities.csv")`, and a shader file read
 *  through it by `material::shader(hub(), "res://aurora.sksl", Params{})`.
 *  A font file is one too: `hub().load<weave::Face>(uri)` answers the
 *  face it holds, and the session that loaded it knows that face by its
 *  family until it closes, so `fontFamily("Muthur Study")` finds it as it
 *  finds an installed family.
 *  A URI is spelled whole; a bare name is a path from where the process
 *  started, never a resource. A file a load found missing or unreadable
 *  is watched, and `poll()` says when it appears. */
class Assets {
 public:
  /** WHILE IT STANDS, every face the hub reads on the thread it was made
   *  on is adopted by one font context under its family, with the URI it
   *  was read from, and held there: each face as the hub reads it, a file
   *  edited on disk included, whose new face replaces the old. Ended, it
   *  lets go of every face it holds, and a face no standing adoption over
   *  the context still holds leaves it, so the family answers no more.
   *  Moved, it keeps holding. */
  class FaceAdoption {
   public:
    FaceAdoption(FaceAdoption&& other) noexcept;
    FaceAdoption& operator=(FaceAdoption&& other) noexcept;
    FaceAdoption(const FaceAdoption&) = delete;
    FaceAdoption& operator=(const FaceAdoption&) = delete;
    ~FaceAdoption();

   private:
    friend class Assets;
    struct Faces;
    struct Holder;
    FaceAdoption(std::shared_ptr<Faces> faces, std::shared_ptr<Holder> holder);
    std::shared_ptr<Faces> m_faces;
    std::shared_ptr<Holder> m_holder;
  };

  /** @p root mounts at `res://`; @p sketches, the directory the sketch
   *  sources stand in, mounts at `sketch://`, under which a sketch's own
   *  files are `sketch://<key>/name`. */
  explicit Assets(std::filesystem::path root,
                  std::filesystem::path sketches = {});
  /** Mounts @p directory as the files of the sketch keyed @p key — what a
   *  workspace sketch opened by path needs, since its files stand beside
   *  that path and not under the sketches the build compiled. */
  void mountSketch(std::string_view key, std::filesystem::path directory);

  /** The resource hub with the assets directory mounted at `res://` and
   *  the sketches at `sketch://`. */
  sigil::io::Hub& hub() { return m_hub; }

  /** MAKES EVERY FONT THIS HUB LOADS FROM HERE ON KNOWN TO @p fonts BY
   *  ITS FAMILY for as long as the answer stands — what a session does when
   *  it opens, so a sketch that loads a font file names its family anywhere
   *  after, and the family leaves @p fonts when the session closes. Every
   *  face the hub decoded before is let go of, so a file the new session
   *  loads is read again and arrives for it: two sessions that load one
   *  file share one face, and a session that loads none finds no family
   *  another session loaded, once that one has closed.
   *  @trap A face is adopted by the contexts adopting on the thread that
   *  decoded it — the thread that asked the hub for it or polled it — since
   *  a font context belongs to one thread. */
  [[nodiscard]] FaceAdoption adoptFacesInto(weave::FontContext& fonts);

  /** Re-checks everything: true when a loaded resource changed on disk
   *  or a file a load found missing appeared (the host re-runs setup). */
  bool poll();

  const std::filesystem::path& root() const { return m_root; }

 private:
  std::filesystem::path m_root;
  std::filesystem::path m_sketches;
  /** The contexts adopting the faces the hub reads, shared with the hub's
   *  face decoder. */
  std::shared_ptr<FaceAdoption::Faces> m_faces;
  sigil::io::Hub m_hub;
};

/** WHETHER EVERY ONE OF @p urls IS ALREADY ON THIS MACHINE, in the
 *  IO hub's network cache — the probe a sketch over fetched art writes
 *  into its `available()`.
 *
 *  Such a sketch keeps a procedural stand-in at every use site, so a
 *  cold cache still renders; but it renders the STAND-IN, and the plate
 *  the sketch is judged on is then not the picture its header describes.
 *  Two plates under one name is the one thing a byte-identity sweep
 *  cannot survive, so the sketch is unavailable BY NAME until the art is
 *  here: a machine that has fetched once stays available offline forever
 *  after, and one that never has stands down with the first missing URL
 *  as the reason, in @p why when it is given.
 *
 *  It asks the cache the IO hub's own way — the file a URL lands under,
 *  in the directory fetches persist to — and never the network, because
 *  a probe that fetched would make availability a function of the
 *  connection. @p cacheDirectory names another cache than the IO hub's default,
 *  which is what a test hands it.
 *
 *  THE SPAN IS THE GENERAL FORM, for a caller whose URLs are decided
 *  while it runs — a host asking on behalf of a sketch written in
 *  another language, where the list is a value and not a literal. */
[[nodiscard]] bool requireCached(
    std::span<const std::string_view> urls, std::string* why,
    const std::filesystem::path& cacheDirectory = {});

/** The same probe over a list written where it is asked, which is the
 *  shape a sketch's own `available()` spells. */
[[nodiscard]] bool requireCached(
    std::initializer_list<std::string_view> urls, std::string* why,
    const std::filesystem::path& cacheDirectory = {});

}  // namespace sigil::sketch
