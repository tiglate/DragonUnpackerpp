// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/HyperRipper.h"

#include <QDialog>

class QCheckBox;
class QSpinBox;

namespace Dup::Ui
{
/// The options of the HyperRipper for MP3 and AAC: the layers of each MPEG version, limits of frames and size, Xing and ID3.
class MpegOptionsDialog : public QDialog
{
    Q_OBJECT

  public:
    MpegOptionsDialog(const Core::MpegOptions &options, QWidget *parent = nullptr);

    [[nodiscard]] auto GetOptions() const -> Core::MpegOptions;

  private:
    QCheckBox *_layers[3][3];
    QCheckBox *_framesMin;
    QSpinBox *_framesMinValue;
    QCheckBox *_framesMax;
    QSpinBox *_framesMaxValue;
    QCheckBox *_sizeMin;
    QSpinBox *_sizeMinValue;
    QCheckBox *_sizeMax;
    QSpinBox *_sizeMaxValue;
    QCheckBox *_xing;
    QCheckBox *_id3;
};
} // namespace Dup::Ui
