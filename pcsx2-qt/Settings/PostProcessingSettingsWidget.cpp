// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "PostProcessingSettingsWidget.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include <QtConcurrent/QtConcurrent>

#include "pcsx2/GS/GS.h"
#include "pcsx2/Host.h"
#include "pcsx2/INISettingsInterface.h"
#include "common/Error.h"

#ifdef ENABLE_LIBRASHADER
#include <librashader/librashader.h>
#endif

static constexpr int DEFAULT_TV_SHADER_MODE = 0;
static constexpr int DEFAULT_CAS_SHARPNESS = 50;
static constexpr int LIBRASHADER_SLIDER_STEPS = 10000;
static constexpr size_t LIBRASHADER_PARAMS_PER_PAGE = 250;

namespace
{
	QStringList ParseSlangpPassDisplayNames(const QString& preset_path)
	{
		QFile f(preset_path);
		if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
			return {};

		const QString all = QString::fromUtf8(f.readAll());
		const QStringList lines = all.split('\n');
		static const QRegularExpression re(QStringLiteral(R"(^\s*shader(\d+)\s*=\s*(.+)$)"));
		QMap<int, QString> by_index;
		for (const QString& line : lines)
		{
			const QRegularExpressionMatch m = re.match(line);
			if (!m.hasMatch())
				continue;
			const int idx = m.captured(1).toInt();
			QString v = m.captured(2).trimmed();
			if (v.startsWith('"') && v.endsWith('"') && v.size() >= 2)
				v = v.mid(1, v.size() - 2).trimmed();
			else if (v.startsWith('\'') && v.endsWith('\'') && v.size() >= 2)
				v = v.mid(1, v.size() - 2).trimmed();
			if (v.isEmpty() || v.startsWith('#'))
				continue;
			by_index.insert(idx, v);
		}

		QStringList out;
		for (auto it = by_index.constBegin(); it != by_index.constEnd(); ++it)
		{
			QString p = it.value();
			const int slash = std::max(p.lastIndexOf('/'), p.lastIndexOf('\\'));
			if (slash >= 0)
				p = p.mid(slash + 1);
			out.append(p);
		}
		if (out.isEmpty())
			out.append(QFileInfo(preset_path).fileName());
		return out;
	}

} // namespace

PostProcessingSettingsWidget::PostProcessingSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupTab(m_post, tr("Post-Processing"));

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_post.fxaa, "EmuCore/GS", "fxaa", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_post.shadeBoost, "EmuCore/GS", "ShadeBoost", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostBrightness, "EmuCore/GS", "ShadeBoost_Brightness", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_BRIGHTNESS);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostContrast, "EmuCore/GS", "ShadeBoost_Contrast", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_CONTRAST);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostGamma, "EmuCore/GS", "ShadeBoost_Gamma", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_GAMMA);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostSaturation, "EmuCore/GS", "ShadeBoost_Saturation", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_SATURATION);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.tvShader, "EmuCore/GS", "TVShader", DEFAULT_TV_SHADER_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.casMode, "EmuCore/GS", "CASMode", static_cast<int>(Pcsx2Config::GSOptions::DEFAULT_CAS_MODE));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.casSharpness, "EmuCore/GS", "CASSharpness", DEFAULT_CAS_SHARPNESS);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_post.librashaderGroup, "EmuCore/GS", "LibrashaderEnabled", false);
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_post.librashaderPreset, "EmuCore/GS", "LibrashaderPreset", std::string());

	connect(m_post.librashaderBrowse, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderBrowse);
	connect(m_post.librashaderClear, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderClear);
	connect(m_post.librashaderRestoreDefaults, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderRestoreDefaults);
	connect(m_post.librashaderPreset, &QLineEdit::textChanged, this, &PostProcessingSettingsWidget::scheduleLibrashaderParamsRebuild);
	connect(m_post.librashaderParamSearch, &QLineEdit::textChanged, this, &PostProcessingSettingsWidget::onLibrashaderParamSearchChanged);
	connect(m_post.librashaderGroup, &QGroupBox::toggled, this, &PostProcessingSettingsWidget::rebuildLibrashaderParamsUi);
#ifdef ENABLE_LIBRASHADER
	connect(m_post.librashaderPrevPage, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderPrevPage);
	connect(m_post.librashaderNextPage, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderNextPage);
#endif
	m_librashader_rebuild_timer = new QTimer(this);
	m_librashader_rebuild_timer->setSingleShot(true);
	m_librashader_rebuild_timer->setInterval(200);
	connect(m_librashader_rebuild_timer, &QTimer::timeout, this, &PostProcessingSettingsWidget::rebuildLibrashaderParamsUi);

	connect(m_post.shadeBoost, &QCheckBox::checkStateChanged, this, &PostProcessingSettingsWidget::onShadeBoostChanged);
	onShadeBoostChanged();

	updateLibrashaderParamsVisibility();
	rebuildLibrashaderParamsUi();

	dialog()->registerWidgetHelp(m_post.librashaderGroup, tr("RetroArch Shader Preset (librashader)"), tr("Unchecked"),
		tr("Enables a RetroArch .slangp shader preset chain using librashader."));

	dialog()->registerWidgetHelp(m_post.librashaderPreset, tr("Preset Path"), tr(""),
		tr("Path to the .slangp shader preset file to load."));

	dialog()->registerWidgetHelp(m_post.librashaderRestoreDefaults, tr("Restore Defaults"), tr(""),
		tr("Resets all shader parameters for the current preset to their default values."));

	dialog()->registerWidgetHelp(m_post.librashaderPassesSectionLabel, tr("Passes"), tr(""),
		tr("Lists shader passes in the order they are declared in the .slangp preset file."));
	dialog()->registerWidgetHelp(m_post.librashaderParamSearch, tr("Parameter Search"), tr(""),
		tr("Filters adjustable parameters by name or description."));

	dialog()->registerWidgetHelp(m_post.librashaderParametersSectionLabel, tr("Parameters"), tr(""),
		tr("Runtime-adjustable parameters exposed by the preset. Values are saved per preset path."));

	//: You might find an official translation for this on AMD's website (Spanish version linked): https://www.amd.com/es/technologies/radeon-software-fidelityfx
	dialog()->registerWidgetHelp(m_post.casMode, tr("Contrast Adaptive Sharpening"), tr("None (Default)"), tr("Enables FidelityFX Contrast Adaptive Sharpening."));

	dialog()->registerWidgetHelp(m_post.casSharpness, tr("Sharpness"), tr("50%"), tr("Determines the intensity the sharpening effect in CAS post-processing."));

	dialog()->registerWidgetHelp(m_post.shadeBoost, tr("Shade Boost"), tr("Unchecked"),
		tr("Enables saturation, contrast, and brightness to be adjusted. Values of brightness, saturation, and contrast are at default "
		   "50."));

	dialog()->registerWidgetHelp(
		m_post.fxaa, tr("FXAA"), tr("Unchecked"), tr("Applies the FXAA anti-aliasing algorithm to improve the visual quality of games."));

	dialog()->registerWidgetHelp(m_post.shadeBoostBrightness, tr("Brightness"), tr("50"), tr("Adjusts brightness. 50 is normal."));

	dialog()->registerWidgetHelp(m_post.shadeBoostContrast, tr("Contrast"), tr("50"), tr("Adjusts contrast. 50 is normal."));

	dialog()->registerWidgetHelp(m_post.shadeBoostGamma, tr("Gamma"), tr("50"), tr("Adjusts gamma. 50 is normal."));

	dialog()->registerWidgetHelp(m_post.shadeBoostSaturation, tr("Saturation"), tr("50"), tr("Adjusts saturation. 50 is normal."));

	dialog()->registerWidgetHelp(m_post.tvShader, tr("TV Shader"), tr("None (Default)"),
		tr("Applies a shader which replicates the visual effects of different styles of television sets."));
}

PostProcessingSettingsWidget::~PostProcessingSettingsWidget() = default;

void PostProcessingSettingsWidget::onShadeBoostChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "ShadeBoost", false);
	m_post.shadeBoostBrightness->setEnabled(enabled);
	m_post.shadeBoostContrast->setEnabled(enabled);
	m_post.shadeBoostGamma->setEnabled(enabled);
	m_post.shadeBoostSaturation->setEnabled(enabled);
}

void PostProcessingSettingsWidget::onLibrashaderBrowse()
{
	QString path = QFileDialog::getOpenFileName(this, tr("Select Shader Preset"), QString(),
		tr("Shader Presets (*.slangp);;All Files (*.*)"));
	if (!path.isEmpty())
	{
		m_post.librashaderPreset->setText(path);
		rebuildLibrashaderParamsUi();
	}
}

void PostProcessingSettingsWidget::onLibrashaderClear()
{
	m_post.librashaderPreset->clear();
	rebuildLibrashaderParamsUi();
}

void PostProcessingSettingsWidget::scheduleLibrashaderParamsRebuild()
{
	if (m_librashader_rebuild_timer)
		m_librashader_rebuild_timer->start();
}

void PostProcessingSettingsWidget::updateLibrashaderParamsVisibility()
{
	const bool show = m_post.librashaderGroup->isChecked();
	const QString preset_path = m_post.librashaderPreset->text().trimmed();
	const bool has_preset = show && !preset_path.isEmpty() && QFile::exists(preset_path);
	m_post.librashaderPassesSectionLabel->setVisible(show);
	m_post.librashaderPassesSectionLabel->setEnabled(has_preset);
	m_post.librashaderPassList->setVisible(show);
	m_post.librashaderPassList->setEnabled(has_preset);
	m_post.librashaderSeparator->setVisible(show);
	m_post.librashaderParametersSectionLabel->setVisible(show);
	m_post.librashaderParametersSectionLabel->setEnabled(has_preset);
	m_post.librashaderParamSearch->setVisible(show);
	m_post.librashaderParamSearch->setEnabled(has_preset);
	m_post.librashaderPrevPage->setVisible(show && has_preset);
	m_post.librashaderPrevPage->setEnabled(has_preset);
	m_post.librashaderPageLabel->setVisible(show && has_preset);
	m_post.librashaderPageLabel->setEnabled(has_preset);
	m_post.librashaderNextPage->setVisible(show && has_preset);
	m_post.librashaderNextPage->setEnabled(has_preset);
	m_post.librashaderParamsScrollArea->setVisible(show);
	m_post.librashaderParamsScrollArea->setEnabled(has_preset);
#ifdef ENABLE_LIBRASHADER
	m_post.librashaderRestoreDefaults->setVisible(show);
	m_post.librashaderRestoreDefaults->setEnabled(false);
#else
	m_post.librashaderRestoreDefaults->setVisible(false);
#endif
}

void PostProcessingSettingsWidget::clearLibrashaderParamsLayout()
{
	QLayout* layout = m_post.librashaderParamsGridLayout;
	while (QLayoutItem* item = layout->takeAt(0))
	{
		if (QWidget* w = item->widget())
			delete w;
		else if (QLayout* l = item->layout())
			delete l;
		delete item;
	}
}

void PostProcessingSettingsWidget::saveLibrashaderParamsToSettings()
{
#ifdef ENABLE_LIBRASHADER
	const QString preset_path = m_post.librashaderPreset->text().trimmed();
	if (preset_path.isEmpty())
		return;

	{
		const std::string section = Pcsx2Config::GSOptions::GetLibrashaderParamsSectionName(preset_path.toStdString());
		INISettingsInterface params_si(Pcsx2Config::GSOptions::GetLibrashaderParamsFilePath());
		params_si.Load();
		params_si.ClearSection(section.c_str());
		for (const LibrashaderPresetParameter& param : m_librashader_params)
		{
			params_si.SetFloatValue(section.c_str(), param.name.toUtf8().constData(), param.value_v);
		}
		Error err;
		params_si.Save(&err);
	}

	if (dialog()->getSettingsInterface())
		g_emu_thread->reloadGameSettings();
	else
		g_emu_thread->applySettings();
#endif
}

void PostProcessingSettingsWidget::onLibrashaderRestoreDefaults()
{
#ifdef ENABLE_LIBRASHADER
	for (LibrashaderPresetParameter& param : m_librashader_params)
		param.value_v = param.initial_v;
	renderLibrashaderParamsPage();
	saveLibrashaderParamsToSettings();
#endif
}

void PostProcessingSettingsWidget::populateLibrashaderPassList(const QString& preset_path)
{
	m_post.librashaderPassList->clear();
	if (preset_path.isEmpty() || !QFile::exists(preset_path))
		return;

	const QStringList names = ParseSlangpPassDisplayNames(preset_path);
	for (const QString& name : names)
		new QListWidgetItem(name, m_post.librashaderPassList);
	if (m_post.librashaderPassList->count() == 0)
		new QListWidgetItem(QFileInfo(preset_path).fileName(), m_post.librashaderPassList);
	if (m_post.librashaderPassList->count() > 0)
		m_post.librashaderPassList->setCurrentRow(0);
}

#ifdef ENABLE_LIBRASHADER
void PostProcessingSettingsWidget::populateLibrashaderPassListFromNames(const QStringList& names, const QString& fallback_preset_path)
{
	QSignalBlocker blocker(m_post.librashaderPassList);
	m_post.librashaderPassList->clear();

	for (const QString& name : names)
		new QListWidgetItem(name, m_post.librashaderPassList);

	if (m_post.librashaderPassList->count() == 0 && !fallback_preset_path.isEmpty())
		new QListWidgetItem(QFileInfo(fallback_preset_path).fileName(), m_post.librashaderPassList);
	if (m_post.librashaderPassList->count() > 0)
		m_post.librashaderPassList->setCurrentRow(0);
}

void PostProcessingSettingsWidget::startLibrashaderParamsLoad(const QString& preset_path)
{
	const u64 request_id = ++m_librashader_load_request_id;
	QFuture<LibrashaderPresetLoadResult> future = QtConcurrent::run([preset_path]() {
		LibrashaderPresetLoadResult result;
		result.pass_names = ParseSlangpPassDisplayNames(preset_path);

		libra_shader_preset_t preset = nullptr;
		libra_error_t err = libra_preset_create(preset_path.toUtf8().constData(), &preset);
		if (err)
		{
			libra_error_free(&err);
			result.error_message = PostProcessingSettingsWidget::tr("Could not load the shader preset. Check that the path is valid and points to a .slangp file.");
			return result;
		}

		libra_preset_param_list_t list = {};
		err = libra_preset_get_runtime_params(&preset, &list);
		if (err)
		{
			libra_error_free(&err);
			libra_preset_free(&preset);
			result.error_message = PostProcessingSettingsWidget::tr("Could not read shader parameters from this preset.");
			return result;
		}

		std::unordered_map<std::string, float> saved_for_preset;
		{
			const std::string section = Pcsx2Config::GSOptions::GetLibrashaderParamsSectionName(preset_path.toStdString());
			INISettingsInterface params_si(Pcsx2Config::GSOptions::GetLibrashaderParamsFilePath());
			params_si.Load();
			for (const auto& [key, val] : params_si.GetKeyValueList(section.c_str()))
			{
				char* end;
				const float f = std::strtof(val.c_str(), &end);
				if (end != val.c_str())
					saved_for_preset.emplace(key, f);
			}
		}

		result.parameters.reserve(static_cast<size_t>(list.length));
		for (uint64_t i = 0; i < list.length; ++i)
		{
			const libra_preset_param_t& p = list.parameters[i];
			const QString name = QString::fromUtf8(p.name ? p.name : "").trimmed();
			if (name.isEmpty())
				continue;

			LibrashaderPresetParameter param;
			param.name = name;
			param.description = QString::fromUtf8(p.description ? p.description : "");
			param.min_v = p.minimum;
			param.max_v = p.maximum;
			param.step_v = p.step;
			param.initial_v = p.initial;
			param.is_bool = (param.min_v == 0.0f && param.max_v == 1.0f && param.step_v >= 1.0f - 1e-5f);

			const auto it = saved_for_preset.find(name.toStdString());
			param.value_v = (it != saved_for_preset.end()) ? it->second : param.initial_v;
			result.parameters.push_back(std::move(param));
		}

		libra_preset_free_runtime_params(list);
		libra_preset_free(&preset);
		result.success = true;
		return result;
	});

	future.then(this, [this, request_id, preset_path](LibrashaderPresetLoadResult result) {
		applyLibrashaderParamsLoadResult(request_id, preset_path, std::move(result));
	});
}

void PostProcessingSettingsWidget::applyLibrashaderParamsLoadResult(u64 request_id, const QString& preset_path,
	LibrashaderPresetLoadResult&& result)
{
	if (request_id != m_librashader_load_request_id)
		return;
	if (!m_post.librashaderGroup->isChecked())
		return;
	if (m_post.librashaderPreset->text().trimmed() != preset_path)
		return;

	clearLibrashaderParamsLayout();
	QGridLayout* const grid = m_post.librashaderParamsGridLayout;
	const auto showMessage = [grid](const QString& text) {
		QLabel* msg = new QLabel(text);
		msg->setWordWrap(true);
		grid->addWidget(msg, 0, 0, 1, 3);
	};

	populateLibrashaderPassListFromNames(result.pass_names, preset_path);
	if (!result.success)
	{
		m_librashader_params.clear();
		m_librashader_page_index = 0;
		m_post.librashaderRestoreDefaults->setEnabled(false);
		m_post.librashaderPageLabel->setText(tr("Page 1/1"));
		m_post.librashaderPrevPage->setVisible(false);
		m_post.librashaderNextPage->setVisible(false);
		showMessage(result.error_message);
		return;
	}

	m_librashader_params = std::move(result.parameters);
	m_librashader_page_index = 0;
	m_post.librashaderRestoreDefaults->setEnabled(true);
	renderLibrashaderParamsPage();
}

void PostProcessingSettingsWidget::renderLibrashaderParamsPage()
{
	clearLibrashaderParamsLayout();
	QGridLayout* const grid = m_post.librashaderParamsGridLayout;

	if (m_librashader_params.empty())
	{
		m_post.librashaderPageLabel->setText(tr("Page 1/1"));
		m_post.librashaderPrevPage->setVisible(false);
		m_post.librashaderNextPage->setVisible(false);
		QLabel* msg = new QLabel(tr("This preset does not expose any adjustable parameters."));
		msg->setWordWrap(true);
		grid->addWidget(msg, 0, 0, 1, 3);
		grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), 1, 0, 1, 3);
		return;
	}

	const QString query = m_post.librashaderParamSearch->text().trimmed();
	std::vector<size_t> visible_indices;
	visible_indices.reserve(m_librashader_params.size());
	for (size_t index = 0; index < m_librashader_params.size(); index++)
	{
		const LibrashaderPresetParameter& p = m_librashader_params[index];
		if (query.isEmpty() || p.name.contains(query, Qt::CaseInsensitive) || p.description.contains(query, Qt::CaseInsensitive))
			visible_indices.push_back(index);
	}

	if (visible_indices.empty())
	{
		m_post.librashaderPageLabel->setText(tr("Page 1/1 (0 parameters)"));
		m_post.librashaderPrevPage->setVisible(false);
		m_post.librashaderNextPage->setVisible(false);
		QLabel* msg = new QLabel(tr("No parameters match the current search."));
		msg->setWordWrap(true);
		grid->addWidget(msg, 0, 0, 1, 3);
		grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), 1, 0, 1, 3);
		return;
	}

	const size_t total_params = visible_indices.size();
	const size_t total_pages = (total_params + LIBRASHADER_PARAMS_PER_PAGE - 1) / LIBRASHADER_PARAMS_PER_PAGE;
	m_librashader_page_index = std::min(m_librashader_page_index, total_pages - 1);
	const size_t start = m_librashader_page_index * LIBRASHADER_PARAMS_PER_PAGE;
	const size_t end = std::min(start + LIBRASHADER_PARAMS_PER_PAGE, total_params);

	int grid_row = 0;
	const QString page_text = tr("Page %1/%2 (%3 parameters)")
	                              .arg(static_cast<int>(m_librashader_page_index + 1))
	                              .arg(static_cast<int>(total_pages))
	                              .arg(static_cast<int>(total_params));
	m_post.librashaderPageLabel->setText(page_text);
	m_post.librashaderPrevPage->setVisible(m_post.librashaderGroup->isChecked() && total_pages > 1);
	m_post.librashaderNextPage->setVisible(m_post.librashaderGroup->isChecked() && total_pages > 1);
	m_post.librashaderPrevPage->setEnabled(m_librashader_page_index > 0);
	m_post.librashaderNextPage->setEnabled((m_librashader_page_index + 1) < total_pages);

	for (size_t visible_index = start; visible_index < end; ++visible_index)
	{
		const size_t index = visible_indices[visible_index];
		const LibrashaderPresetParameter& p = m_librashader_params[index];
		if (p.is_bool)
		{
			QCheckBox* const cb = new QCheckBox(p.name);
			cb->setChecked(p.value_v >= 0.5f);
			cb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
			if (!p.description.isEmpty())
				cb->setToolTip(p.description);
			grid->addWidget(cb, grid_row, 0, 1, 3, Qt::AlignLeft);
			connect(cb, &QCheckBox::checkStateChanged, this, [this, index](Qt::CheckState state) {
				m_librashader_params[index].value_v = (state == Qt::Checked) ? 1.0f : 0.0f;
				saveLibrashaderParamsToSettings();
			});
			grid_row++;
		}
		else
		{
			QLabel* const name_label = new QLabel(p.name);
			name_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
			name_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
			if (!p.description.isEmpty())
				name_label->setToolTip(p.description);

			QSlider* const slider = new QSlider(Qt::Horizontal);
			slider->setRange(0, LIBRASHADER_SLIDER_STEPS);
			slider->setValue(librashaderValueToSlider(p.value_v, p.min_v, p.max_v, p.step_v));
			slider->setSingleStep(LIBRASHADER_SLIDER_STEPS / 100);
			slider->setPageStep(LIBRASHADER_SLIDER_STEPS / 10);
			slider->setMinimumHeight(22);
			slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			slider->setFocusPolicy(Qt::StrongFocus);

			const float shown = sliderToLibrashaderValue(slider->value(), p.min_v, p.max_v, p.step_v);
			QLabel* const value_label = new QLabel(formatLibrashaderValue(shown, p.min_v, p.max_v, p.step_v));
			value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			value_label->setFixedWidth(62);
			value_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

			grid->addWidget(name_label, grid_row, 0, Qt::AlignLeft | Qt::AlignVCenter);
			grid->addWidget(slider, grid_row, 1, Qt::AlignVCenter);
			grid->addWidget(value_label, grid_row, 2, Qt::AlignRight | Qt::AlignVCenter);

			connect(slider, &QSlider::valueChanged, this, [this, index, slider, value_label](int) {
				LibrashaderPresetParameter& param = m_librashader_params[index];
				const float current = sliderToLibrashaderValue(slider->value(), param.min_v, param.max_v, param.step_v);
				param.value_v = current;
				value_label->setText(formatLibrashaderValue(current, param.min_v, param.max_v, param.step_v));
			});
			connect(slider, &QSlider::sliderReleased, this, &PostProcessingSettingsWidget::saveLibrashaderParamsToSettings);

			grid_row++;
		}
	}

	grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), grid_row, 0, 1, 3);
}

void PostProcessingSettingsWidget::onLibrashaderPrevPage()
{
	if (m_librashader_page_index == 0)
		return;
	m_librashader_page_index--;
	renderLibrashaderParamsPage();
}

void PostProcessingSettingsWidget::onLibrashaderNextPage()
{
	if ((m_librashader_page_index + 1) * LIBRASHADER_PARAMS_PER_PAGE >= m_librashader_params.size())
		return;
	m_librashader_page_index++;
	renderLibrashaderParamsPage();
}
#endif

void PostProcessingSettingsWidget::rebuildLibrashaderParamsUi()
{
	if (m_librashader_rebuild_timer)
		m_librashader_rebuild_timer->stop();
	updateLibrashaderParamsVisibility();
	clearLibrashaderParamsLayout();
	QGridLayout* const grid = m_post.librashaderParamsGridLayout;
	const auto showMessage = [grid](const QString& text) {
		QLabel* msg = new QLabel(text);
		msg->setWordWrap(true);
		grid->addWidget(msg, 0, 0, 1, 3);
	};

	if (!m_post.librashaderGroup->isChecked())
	{
		++m_librashader_load_request_id;
#ifdef ENABLE_LIBRASHADER
		m_librashader_params.clear();
		m_librashader_page_index = 0;
#endif
		m_post.librashaderPageLabel->setText(tr("Page 1/1"));
		m_post.librashaderPassList->clear();
		return;
	}

	const QString preset_path = m_post.librashaderPreset->text().trimmed();
#ifndef ENABLE_LIBRASHADER
	populateLibrashaderPassList(preset_path);
	showMessage(tr("Shader preset parameters are not available this build was compiled without librashader."));
	return;
#else
	m_post.librashaderPassList->clear();
	if (preset_path.isEmpty())
	{
		++m_librashader_load_request_id;
		m_librashader_params.clear();
		m_librashader_page_index = 0;
		m_post.librashaderRestoreDefaults->setEnabled(false);
		m_post.librashaderPageLabel->setText(tr("Page 1/1"));
		showMessage(tr("Select a .slangp preset to view runtime parameters."));
		return;
	}

	if (!QFile::exists(preset_path))
	{
		++m_librashader_load_request_id;
		m_librashader_params.clear();
		m_librashader_page_index = 0;
		m_post.librashaderRestoreDefaults->setEnabled(false);
		m_post.librashaderPageLabel->setText(tr("Page 1/1"));
		showMessage(tr("The selected preset file does not exist."));
		return;
	}

	m_post.librashaderRestoreDefaults->setEnabled(false);
	m_post.librashaderPageLabel->setText(tr("Loading..."));
	showMessage(tr("Loading shader parameters..."));
	startLibrashaderParamsLoad(preset_path);
#endif
}

void PostProcessingSettingsWidget::onLibrashaderParamSearchChanged(const QString& text)
{
	Q_UNUSED(text);
#ifdef ENABLE_LIBRASHADER
	m_librashader_page_index = 0;
	renderLibrashaderParamsPage();
#endif
}

#ifdef ENABLE_LIBRASHADER
float PostProcessingSettingsWidget::snapLibrashaderValue(float value, float min_v, float max_v, float step_v) const
{
	if (step_v > 0.0f)
		value = min_v + std::round((value - min_v) / step_v) * step_v;
	return std::clamp(value, min_v, max_v);
}

float PostProcessingSettingsWidget::sliderToLibrashaderValue(int slider_value, float min_v, float max_v, float step_v) const
{
	const float t = static_cast<float>(slider_value) / static_cast<float>(LIBRASHADER_SLIDER_STEPS);
	const float value = min_v + (max_v - min_v) * t;
	return snapLibrashaderValue(value, min_v, max_v, step_v);
}

int PostProcessingSettingsWidget::librashaderValueToSlider(float value, float min_v, float max_v, float step_v) const
{
	if (std::abs(max_v - min_v) < 1e-8f)
		return 0;

	value = snapLibrashaderValue(value, min_v, max_v, step_v);
	const float t = (value - min_v) / (max_v - min_v);
	return std::clamp(static_cast<int>(std::lround(t * static_cast<float>(LIBRASHADER_SLIDER_STEPS))), 0, LIBRASHADER_SLIDER_STEPS);
}

QString PostProcessingSettingsWidget::formatLibrashaderValue(float value, float min_v, float max_v, float step_v) const
{
	value = snapLibrashaderValue(value, min_v, max_v, step_v);
	if (step_v >= 1.0f - 1e-6f)
		return QString::number(static_cast<int>(std::lround(value)));
	if (step_v >= 0.1f - 1e-6f)
		return QString::number(value, 'f', 1);
	if (step_v >= 0.01f - 1e-6f)
		return QString::number(value, 'f', 2);
	if (step_v > 0.0f)
		return QString::number(value, 'f', 3);

	const float span = std::abs(max_v - min_v);
	if (span <= 1.0f + 1e-6f && min_v >= 0.0f && max_v <= 1.0f + 1e-6f)
		return QString::number(value, 'f', 2);
	if (span >= 100.0f)
		return QString::number(value, 'f', 1);
	return QString::number(value, 'g', 5);
}
#endif

#include "moc_PostProcessingSettingsWidget.cpp"
