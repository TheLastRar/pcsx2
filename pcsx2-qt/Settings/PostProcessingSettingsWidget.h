// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "SettingsWidget.h"
#include "ui_PostProcessingSettingsWidget.h"

class QCheckBox;
class QLabel;
class QSlider;
class QTimer;

class PostProcessingSettingsWidget final : public SettingsWidget
{
	Q_OBJECT

public:
	PostProcessingSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~PostProcessingSettingsWidget();

private Q_SLOTS:
	void onShadeBoostChanged();
	void onLibrashaderBrowse();
	void onLibrashaderClear();
	void onLibrashaderRestoreDefaults();
	void rebuildLibrashaderParamsUi();
	void onLibrashaderParamSearchChanged(const QString& text);

private:
	void saveLibrashaderParamsToSettings();
	void clearLibrashaderParamsLayout();
	void updateLibrashaderParamsVisibility();
	void populateLibrashaderPassList(const QString& preset_path);
	void scheduleLibrashaderParamsRebuild();
	u64 m_librashader_load_request_id = 0;

#ifdef ENABLE_LIBRASHADER
	struct LibrashaderPresetParameter
	{
		QString name;
		QString description;
		bool is_bool = false;
		float min_v = 0.0f;
		float max_v = 1.0f;
		float step_v = 0.0f;
		float initial_v = 0.0f;
		float value_v = 0.0f;
	};

	struct LibrashaderPresetLoadResult
	{
		QStringList pass_names;
		QString error_message;
		std::vector<LibrashaderPresetParameter> parameters;
		bool success = false;
	};

	float snapLibrashaderValue(float value, float min_v, float max_v, float step_v) const;
	float sliderToLibrashaderValue(int slider_value, float min_v, float max_v, float step_v) const;
	int librashaderValueToSlider(float value, float min_v, float max_v, float step_v) const;
	QString formatLibrashaderValue(float value, float min_v, float max_v, float step_v) const;
	void startLibrashaderParamsLoad(const QString& preset_path);
	void applyLibrashaderParamsLoadResult(u64 request_id, const QString& preset_path, LibrashaderPresetLoadResult&& result);
	void populateLibrashaderPassListFromNames(const QStringList& names, const QString& fallback_preset_path);
	void renderLibrashaderParamsPage();
	void onLibrashaderPrevPage();
	void onLibrashaderNextPage();
	std::vector<LibrashaderPresetParameter> m_librashader_params;
	size_t m_librashader_page_index = 0;
#endif

	QTimer* m_librashader_rebuild_timer = nullptr;
	Ui::PostProcessingSettingsWidget m_post;
};
