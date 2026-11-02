#pragma once
#include <stdbool.h>

void wifi_start(void);            /* start wifi; verbindt daarna zelf steeds opnieuw */
void wifi_wait_connected(void);   /* blokkeert tot er een IP-adres is */

/* true met de signaalsterkte in dBm als er verbinding is, anders false. */
bool wifi_get_rssi(int *rssi);
