"use strict"

const manifest = {
  name: "&Recorder...",
  applicable: app.mediaEncodingAvailable,
}

class Script {
  constructor() {
    this.configurations = app.mediaEncoderConfigurations()
    this.source = null
    this.encoder = null
    this.ui = {}
    this.error = ""
    this.settings = null
    this.nextFrame = 0
    this.nextSample = 0
    this.startSample = 0
    this.endSample = 0
    this.totalFrames = 0

    this.textures = [{ key: 0, text: "None (audio only)", item: null }]
    for (const texture of app.findItems(item => item.type == "Texture")) {
      this.textures.push({
        key: Number(texture.id),
        text: String(texture.name),
        item: texture,
      })
    }
    this.sessionHasAudio = app.findItems(item =>
      item.type == "Call" && item.checked
      && item.callType == "ComputeSound").length > 0
      || app.findItems(item => item.type == "Texture"
        && (item.sourceType == "AudioSpectrum"
          || item.sourceType == "AudioSamples")
        && /\.(wav|mp3|flac|aac|aif|aiff|m4a|ogg|oga|opus|wma)$/i
          .test(String(item.fileName))).length > 0
  }

  initializeUi(ui) {
    this.ui = ui
    ui.textureModel = this.textures.map(texture => ({
      key: texture.key,
      text: texture.text,
    }))
    ui.hasAudio = this.sessionHasAudio
  }

  defaultTextureIndex() {
    return this.textures.length > 1 ? 1 : 0
  }

  defaultFileName(hasVideo) {
    let name = app.session ? String(app.session.name) : "recording"
    name = name.replace(/^.*[\\/]/, "").replace(/\.[^.]*$/, "")
    if (!name)
      name = "recording"
    const formats = this.formats(hasVideo)
    const formatKey = this.defaultKey(formats)
    return name + "." + (formats[formatKey]?.suffix || "media")
  }

  formats(hasVideo) {
    return hasVideo
      ? (this.configurations.videoFormats || {})
      : (this.configurations.audioFormats || {})
  }

  entries(object) {
    return Object.keys(object || {}).map(key => ({
      key,
      text: object[key].name,
      default: object[key].default === true,
    }))
  }

  defaultKey(object) {
    const keys = Object.keys(object || {})
    return keys.find(key => object[key].default === true) || keys[0] || ""
  }

  formatEntries(hasVideo) {
    return this.entries(this.formats(hasVideo))
  }

  videoCodecEntries(formatKey) {
    const format = (this.configurations.videoFormats || {})[formatKey]
    return this.entries(format?.videoCodecs)
  }

  audioCodecEntries(formatKey, videoCodecKey, hasVideo) {
    const format = this.formats(hasVideo)[formatKey]
    const codecs = hasVideo
      ? format?.videoCodecs?.[videoCodecKey]?.audioCodecs
      : format?.audioCodecs
    return this.entries(codecs)
  }

  replaceFileSuffix(fileName, formatKey, hasVideo) {
    const suffix = this.formats(hasVideo)[formatKey]?.suffix
    if (!suffix || !fileName)
      return fileName
    const slash = Math.max(fileName.lastIndexOf("/"), fileName.lastIndexOf("\\"))
    const dot = fileName.lastIndexOf(".")
    return (dot > slash ? fileName.substring(0, dot) : fileName) + "." + suffix
  }

  textureResolution(textureId) {
    const texture = this.textures.find(entry => entry.key == textureId)?.item
    if (!texture)
      return ""
    return String(texture.width) + " × " + String(texture.height)
  }

  record(settings) {
    if (this.source || this.encoder)
      return
    this.error = ""
    if (!Number.isFinite(settings.startTime) || settings.startTime < 0
      || !Number.isFinite(settings.endTime) || settings.endTime <= settings.startTime
      || !Number.isFinite(settings.frameRate) || settings.frameRate <= 0)
      return this.fail("Invalid recording time range or frame rate.")
    const hasVideo = Number(settings.textureId) != 0
    const hasAudio = this.sessionHasAudio
    if (!hasVideo && !hasAudio)
      return this.fail("The session has no video or audio to record.")

    this.settings = settings
    this.settings.hasVideo = hasVideo
    this.settings.hasAudio = hasAudio
    this.settings.sampleRate = Math.max(1,
      Number(app.session.audioSampleRate))
    this.totalFrames = Math.max(1, Math.ceil(
      (settings.endTime - settings.startTime) * settings.frameRate - 1e-9))
    this.startSample = this.sampleAtTime(settings.startTime)
    this.endSample = this.sampleAtTime(settings.endTime)
    this.nextSample = this.startSample
    this.nextFrame = 0

    this.ui.running = true
    this.ui.progress = 0
    this.ui.frameCount = this.totalFrames
    this.ui.status = "Preparing recording..."

    try {
      this.source = app.createSessionRenderer({
        textureId: Number(settings.textureId),
        audio: hasAudio,
      })
      this.encoder = app.createMediaEncoder({
        outputFile: settings.outputFile,
        fileFormat: settings.fileFormat,
        videoCodec: hasVideo ? settings.videoCodec : "",
        audioCodec: hasAudio ? settings.audioCodec : "",
        videoFrameRate: settings.frameRate,
        videoBitRate: Math.max(1, settings.videoBitRate) * 1000,
        audioBitRate: Math.max(1, settings.audioBitRate) * 1000,
      })
      if (!this.source || !this.encoder)
        throw new Error("Media recording is not available.")
      this.source.frameReady.connect(frame => this.frameReady(frame))
      this.encoder.frameWritten.connect(() => this.requestNextFrame())
      this.encoder.finished.connect(error => this.encodingFinished(error))
      this.requestNextFrame()
    } catch (error) {
      this.fail(String(error))
    }
  }

  cancel() {
    if (!this.source && !this.encoder)
      return
    this.dispose()
    this.ui.running = false
    this.ui.status = "Recording canceled."
  }

  sampleAtTime(time) {
    return Math.max(0, Math.round(time * this.settings.sampleRate))
  }

  requestNextFrame() {
    if (!this.source || this.nextFrame >= this.totalFrames)
      return
    const index = this.nextFrame++
    const time = this.settings.startTime + index / this.settings.frameRate
    const nextTime = Math.min(this.settings.endTime,
      this.settings.startTime + (index + 1) / this.settings.frameRate)
    const nextSample = index + 1 == this.totalFrames
      ? this.endSample : this.sampleAtTime(nextTime)
    const sampleBase = this.nextSample
    this.nextSample = nextSample
    this.ui.status = (this.settings.hasVideo ? "Rendering frame "
      : "Rendering audio segment ") + (index + 1) + " of "
      + this.totalFrames + "..."
    this.source.requestFrame({
      time,
      frameIndex: Math.round(time * this.settings.frameRate),
      soundSampleBase: sampleBase,
      soundFrameCount: Math.max(0, nextSample - sampleBase),
      videoStartTime: Math.round(index * 1000000 / this.settings.frameRate),
      videoEndTime: index + 1 == this.totalFrames
        ? Math.round((this.settings.endTime - this.settings.startTime) * 1000000)
        : Math.round((index + 1) * 1000000 / this.settings.frameRate),
      audioStartTime: Math.round(
        (sampleBase - this.startSample) * 1000000 / this.settings.sampleRate),
      isFinish: index + 1 == this.totalFrames,
    })
  }

  frameReady(frame) {
    if (!this.encoder)
      return
    if (frame.errorString) {
      this.fail(frame.errorString)
      return
    }
    this.ui.progress = this.nextFrame
    this.ui.status = frame.isFinish
      ? "Finalizing recording..." : "Encoding frame..."
    this.encoder.writeFrame(frame)
  }

  encodingFinished(error) {
    if (!this.encoder)
      return
    const outputFile = this.settings.outputFile
    const hasVideo = this.settings.hasVideo
    this.error = error ? String(error) : ""
    this.dispose()
    this.ui.running = false
    this.ui.status = error ? String(error)
      : (hasVideo
        ? "Recorded " + this.totalFrames + " frames to " + outputFile + "."
        : "Recorded audio to " + outputFile + ".")
  }

  fail(error) {
    this.error = String(error)
    this.dispose()
    if (this.ui) {
      this.ui.running = false
      this.ui.status = String(error)
    }
  }

  dispose() {
    const source = this.source
    const encoder = this.encoder
    this.source = null
    this.encoder = null
    if (source)
      source.close()
    if (encoder)
      encoder.close()
  }
}

this.script = new Script()

if (!this.arguments) {
  app.openEditor("ui.qml").title = "Recorder"
}
else {
  const settings = this.arguments
  const textureId = settings.textureId ?? this.script.textures[this.script.defaultTextureIndex()].key
  const hasVideo = Number(textureId) != 0
  const formats = this.script.formats(hasVideo)
  const fileFormat = settings.fileFormat ?? this.script.defaultKey(formats)
  const format = formats[fileFormat]
  const videoCodec = settings.videoCodec ?? this.script.defaultKey(format?.videoCodecs)
  const audioCodec = settings.audioCodec ?? this.script.defaultKey(
    hasVideo ? format?.videoCodecs?.[videoCodec]?.audioCodecs : format?.audioCodecs)

  this.script.record(Object.assign({
    textureId,
    outputFile: this.script.replaceFileSuffix(this.script.defaultFileName(hasVideo), fileFormat, hasVideo),
    fileFormat,
    videoCodec,
    audioCodec,
    startTime: 0,
    endTime: 10,
    frameRate: 60,
    videoBitRate: 12000,
    audioBitRate: 192,
  }, settings))

  // Keep the encoder alive while callbacks dispose the recording resources.
  const encoder = this.script.encoder
  if (encoder && !encoder.waitForFinished(settings.timeout ?? 120000))
    this.script.fail("Recording timed out.")

  this.result = {
    success: !this.script.error,
    error: this.script.error,
    outputFile: this.script.settings?.outputFile,
  }
  if (this.script.error)
    throw new Error(this.script.error)
}
