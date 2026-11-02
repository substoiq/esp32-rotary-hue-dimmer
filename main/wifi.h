#pragma once

void wifi_start(void);            /* start wifi; verbindt daarna zelf steeds opnieuw */
void wifi_wait_connected(void);   /* blokkeert tot er een IP-adres is */
