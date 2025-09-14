if (exporter->getName() == "Commodore 64 SID (.sid)") {
    ImGui::Separator();
    ImGui::Text("SID Options");

    ImGui::Checkbox("PAL (50Hz)", &cfg.sidPAL);
    ImGui::Checkbox("Use CIA timers", &cfg.sidUseCIA);

    int numSubsongs = (int)song->subsongs.size();
    if (numSubsongs < 1) numSubsongs = 1;

    ImGui::Text("This song has %d subsong(s)", numSubsongs);

    ImGui::SliderInt("Start subsong", &cfg.sidStartSong, 1, numSubsongs);
    ImGui::Checkbox("Export all subsongs", &cfg.sidExportAll);

    if (cfg.sidSelectedSubsongs.empty()) {
        cfg.sidSelectedSubsongs.resize(numSubsongs, 1);
    }

    ImGui::Text("Select subsongs to export:");
    for (int i = 0; i < numSubsongs; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Subsong %d", i + 1);
        ImGui::Checkbox(buf, &cfg.sidSelectedSubsongs[i]);
    }
}
