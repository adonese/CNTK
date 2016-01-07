//
// <copyright company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//

#include "stdafx.h"
#include "ImageReader.h"
#include "commandArgUtil.h"
#include "ImageTransformers.h"
#include "BlockRandomizer.h"
#include "ImageDataDeserializer.h"
#include "FrameModePacker.h"

namespace Microsoft { namespace MSR { namespace CNTK {

    ImageReader::ImageReader(
        const ConfigParameters& parameters,
        ElementType elementType)
        : m_seed(0), m_elementType(elementType)
    {
        InitFromConfig(parameters);
    }

    void ImageReader::InitFromConfig(const ConfigParameters& config)
    {
        m_configHelper = std::make_shared<ImageConfigHelper>(config);
        DataDeserializerPtr deserializer = std::make_shared<ImageDataDeserializer>(m_configHelper, m_elementType);

        TransformerPtr randomizer = std::make_shared<BlockRandomizer>(0, SIZE_MAX, deserializer);

        auto inputs = m_configHelper->GetInputs();
        assert(inputs.size() == 2);
        const auto & features = inputs[m_configHelper->GetFeatureInputIndex()];

        TransformerPtr cropper = std::make_shared<CropTransform>(randomizer, features->id, config(features->name), m_seed);
        TransformerPtr scaler = std::make_shared<ScaleTransform>(cropper, features->id, m_seed, m_elementType == et_float ? CV_32F : CV_64F, config(features->name));
        TransformerPtr mean = std::make_shared<MeanTransform>(scaler, features->id);
        m_transformer = mean;
    }

    std::vector<InputDescriptionPtr> ImageReader::GetInputs()
    {
        return m_configHelper->GetInputs();
    }

    void ImageReader::StartEpoch(const EpochConfiguration& config)
    {
        assert(config.minibatchSize > 0);
        assert(config.totalSize > 0);

        m_transformer->SetEpochConfiguration(config);
        m_packer = std::make_shared<FrameModePacker>(
            m_transformer, 
            config.minibatchSize, 
            m_elementType == et_float ? sizeof(float) : sizeof(double),
            m_configHelper->GetInputs());
    }

    Minibatch ImageReader::ReadMinibatch()
    {
        return m_packer->ReadMinibatch();
    }
}}}