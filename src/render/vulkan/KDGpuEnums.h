#pragma once

#include "session/Item.h"
#include "VKContext.h"

KDGpu::Format toKDGpu(QOpenGLTexture::TextureFormat format);
KDGpu::Format toKDGpu(Field::DataType dataType, int count);
KDGpu::Format toKDGpu(Binding::ImageFormat format);
KDGpu::PrimitiveTopology toKDGpu(Call::PrimitiveType primitiveType);
KDGpu::CullModeFlags toKDGpu(Target::CullMode cullMode);
KDGpu::FrontFace toKDGpu(Target::FrontFace frontFace);
KDGpu::PolygonMode toKDGpu(Target::PolygonMode polygonMode);
KDGpu::BlendOperation toKDGpu(Attachment::BlendEquation eq);
KDGpu::BlendFactor toKDGpu(Attachment::BlendFactor factor);
KDGpu::CompareOperation toKDGpu(Attachment::ComparisonFunc func);
KDGpu::StencilOperation toKDGpu(Attachment::StencilOperation op);
KDGpu::SampleCountFlagBits getKDSampleCount(int samples);
