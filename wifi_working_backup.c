// Working WiFi functions from commit 1bb93698da11b2c4455e46c74cbced2a1ae3ee57
// KEEP CREDENTIALS: WIFI_SSID = "KAMNET_8960", WIFI_PASS = "Pawianywchodzanasciany", HOST = "192.168.55.252"

static bool wifi_connect_and_log(void) {
      printf("[BOOT] Inicjalizacja CYW43...\n");

    int init_result = cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND);
      if (init_result != 0) {
          printf("[CYW43] Błąd inicjalizacji: %d\n", init_result);
          return false;
      }
      
      printf("[CYW43] Inicjalizacja pomyślna\n");

      // Stabilizacja po inicjalizacji
      sleep_ms(500);

    cyw43_arch_enable_sta_mode();

      // Dodatkowa stabilizacja po włączeniu trybu STA
      sleep_ms(200);

      const uint32_t try_auths[] = { CYW43_AUTH_WPA2_AES_PSK, CYW43_AUTH_WPA2_MIXED_PSK, CYW43_AUTH_WPA_TKIP_PSK };
      for (size_t i = 0; i < sizeof(try_auths)/sizeof(try_auths[0]); ++i) {
          printf("[WiFi] proba polaczenia (auth=0x%08lx) do \"%s\"...\n", (unsigned long)try_auths[i], WIFI_SSID);

          // Dodatkowa stabilizacja przed próbą połączenia
          sleep_ms(100);

        int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, try_auths[i], 25000); // Oryginalny timeout jak w working version
          if (rc) {
              printf("[WiFi] NIE polaczono (rc=%d)\n", rc);

              // Sprawdź czy to błąd CYW43
              if (rc == PICO_ERROR_GENERIC || rc == PICO_ERROR_TIMEOUT) {
                  printf("[WiFi] Możliwy błąd komunikacji z CYW43 - czekam\n");
                  sleep_ms(500);
              }
              continue;
          }

          struct netif* nif = get_nif();
          if (!nif || !netif_is_up(nif)) { 
              printf("[WiFi] interfejs nie jest UP\n"); 
              continue;
          }

          printf("[WiFi] Połączono z \"%s\"  IP:", WIFI_SSID);
          print_ip4(netif_ip4_addr(nif));
          printf("  GW: "); print_ip4(netif_ip4_gw(nif));
          printf("  MASK: "); print_ip4(netif_ip4_netmask(nif));
          printf("\n");

          int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
          if (rssi != 0) printf("[WiFi] RSSI: %d dBm\n", rssi);
          
      // Dodatkowa stabilizacja po udanym połączeniu
      sleep_ms(200);

      return true;
      }

      printf("[CYW43] Wszystkie próby połączenia nieudane\n");
      return false;
  }