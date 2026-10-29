// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/MpegOptionsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Dup::Ui
{
MpegOptionsDialog::MpegOptionsDialog(const Core::MpegOptions &options, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("MPEG audio options (MP3 and AAC)"));
    auto *layout = new QVBoxLayout(this);

    auto *versions = new QGroupBox(tr("MP3: versions and layers to look for"), this);
    auto *grid = new QGridLayout(versions);
    const char *layerNames[3] = {"Layer I", "Layer II", "Layer III"};
    const char *versionNames[3] = {"MPEG-1", "MPEG-2", "MPEG-2.5 (unofficial)"};
    for (int layer = 0; layer < 3; ++layer)
    {
        grid->addWidget(new QLabel(tr(layerNames[layer]), this), 0, layer + 1);
    }
    for (int version = 0; version < 3; ++version)
    {
        grid->addWidget(new QLabel(tr(versionNames[version]), this), version + 1, 0);
        for (int layer = 0; layer < 3; ++layer)
        {
            _layers[version][layer] = new QCheckBox(this);
            _layers[version][layer]->setChecked(options.Layers[version][layer]);
            grid->addWidget(_layers[version][layer], version + 1, layer + 1);
        }
    }
    layout->addWidget(versions);

    auto *limits = new QGroupBox(tr("Limits (MP3 and AAC)"), this);
    auto *limitGrid = new QGridLayout(limits);
    auto row = [&](int index, const QString &text, bool checked, int value, int maximum, const QString &suffix, QCheckBox *&box, QSpinBox *&spin) {
        box = new QCheckBox(text, this);
        box->setChecked(checked);
        spin = new QSpinBox(this);
        spin->setRange(1, maximum);
        spin->setValue(value);
        spin->setSuffix(suffix);
        spin->setEnabled(checked);
        connect(box, &QCheckBox::toggled, spin, &QWidget::setEnabled);
        limitGrid->addWidget(box, index, 0);
        limitGrid->addWidget(spin, index, 1);
    };
    row(0, tr("At least"), options.LimitFramesMin, static_cast<int>(options.FramesMin), 1000000, tr(" frames"), _framesMin, _framesMinValue);
    row(1, tr("At most (cuts longer files in pieces)"), options.LimitFramesMax, static_cast<int>(options.FramesMax), 1000000, tr(" frames"), _framesMax, _framesMaxValue);
    row(2, tr("At least"), options.LimitSizeMin, static_cast<int>(options.SizeMin), INT32_MAX, tr(" bytes"), _sizeMin, _sizeMinValue);
    row(3, tr("At most"), options.LimitSizeMax, static_cast<int>(options.SizeMax), INT32_MAX, tr(" bytes"), _sizeMax, _sizeMaxValue);
    layout->addWidget(limits);

    auto *special = new QGroupBox(tr("Special"), this);
    auto *specialLayout = new QVBoxLayout(special);
    _xing = new QCheckBox(tr("Trust the size given by the Xing header of variable bit rate files"), this);
    _xing->setChecked(options.XingVbr);
    _id3 = new QCheckBox(tr("Include the ID3 tags (version 2 in front, version 1 at the end)"), this);
    _id3->setChecked(options.Id3Tag);
    specialLayout->addWidget(_xing);
    specialLayout->addWidget(_id3);
    layout->addWidget(special);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

auto MpegOptionsDialog::GetOptions() const -> Core::MpegOptions
{
    Core::MpegOptions options;
    for (int version = 0; version < 3; ++version)
    {
        for (int layer = 0; layer < 3; ++layer)
        {
            options.Layers[version][layer] = _layers[version][layer]->isChecked();
        }
    }
    options.LimitFramesMin = _framesMin->isChecked();
    options.FramesMin = static_cast<std::uint32_t>(_framesMinValue->value());
    options.LimitFramesMax = _framesMax->isChecked();
    options.FramesMax = std::max<std::uint32_t>(static_cast<std::uint32_t>(_framesMaxValue->value()), options.FramesMin);
    options.LimitSizeMin = _sizeMin->isChecked();
    options.SizeMin = _sizeMinValue->value();
    options.LimitSizeMax = _sizeMax->isChecked();
    options.SizeMax = std::max<std::int64_t>(_sizeMaxValue->value(), options.SizeMin);
    options.XingVbr = _xing->isChecked();
    options.Id3Tag = _id3->isChecked();
    return options;
}
} // namespace Dup::Ui
