#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <curl/curl.h>

//https://github.com/Sebasti348/TrabajoPracticoIntegrador_Info1.git

struct memory {
    char *response;
    size_t size;
};

typedef struct {
    long update_id;
    long chat_id;
    long date;
    char name[128];
    char mensaje[512];
} mensajeTelegram;

static size_t cb(void *data, size_t size, size_t nmemb, void *clientp) {
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)clientp;

    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if (ptr == NULL) return 0;

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

long buscar_dato(const char *donde_buscar, const char *clave) {
    if (donde_buscar == NULL) return 0;

    char *pos = strstr(donde_buscar, clave);
    if (pos == NULL) return 0;

    long dato = 0;
    sscanf(pos + strlen(clave), "%ld", &dato);
    return dato;
}

void buscar_texto(const char *donde_buscar, const char *clave, char *destino, int max_tam) {
    destino[0] = '\0';
    if (donde_buscar == NULL) return;

    char *pos = strstr(donde_buscar, clave);
    if (pos != NULL) {
        char formato[20];
        snprintf(formato, sizeof(formato), "%%%d[^\"]", max_tam - 1);
        sscanf(pos + strlen(clave), formato, destino);
    }
}

int rescatarDatos(struct memory chunk, mensajeTelegram *mimsj) {
    if (chunk.response == NULL) return 0;

    char *pos_update = strstr(chunk.response, "\"update_id\":");
    if (pos_update == NULL) {
        return 0; 
    }

    mimsj->update_id = buscar_dato(pos_update, "\"update_id\":");
    mimsj->date = buscar_dato(pos_update, "\"date\":");

    char *pos_chat = strstr(pos_update, "\"chat\":{");
    if (pos_chat != NULL) {
        mimsj->chat_id = buscar_dato(pos_chat, "\"id\":");
        buscar_texto(pos_chat, "\"first_name\":\"", mimsj->name, sizeof(mimsj->name));
    }

    buscar_texto(pos_update, "\"text\":\"", mimsj->mensaje, sizeof(mimsj->mensaje));

    return 1;
}

void guardar_en_txt(const mensajeTelegram *mimsj) {
    FILE *archivo = fopen("historial_chat.txt", "a");
    if (archivo == NULL) {
        perror("Error al abrir historial_chat.txt");
        return;
    }

    fprintf(archivo, "%ld - %s: %s\n", mimsj->date, mimsj->name, mimsj->mensaje);
    fclose(archivo);
}

void enviar_mensaje(CURL *curl, const char *token, long chat_id, const char *texto_plano, mensajeTelegram *resp_bot) {
    char url_envio[512];
    struct memory chunk_envio = {0};

    char *texto_codificado = curl_easy_escape(curl, texto_plano, 0);
    if (texto_codificado == NULL) return;

    snprintf(url_envio, sizeof(url_envio),"https://api.telegram.org/bot%s/sendMessage?chat_id=%ld&text=%s",token, chat_id, texto_codificado);

    curl_free(texto_codificado);

    curl_easy_setopt(curl, CURLOPT_URL, url_envio);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk_envio);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("Error al enviar respuesta: %d\n", res);
    }

    if (chunk_envio.response != NULL) {
        resp_bot->chat_id = chat_id;
        resp_bot->date = buscar_dato(chunk_envio.response, "\"date\":");
        
        char *pos_from = strstr(chunk_envio.response, "\"from\":{");
        if (pos_from != NULL) {
            buscar_texto(pos_from, "\"first_name\":\"", resp_bot->name, sizeof(resp_bot->name));
        } else {
            strcpy(resp_bot->name, "Bot");
        }
        
        snprintf(resp_bot->mensaje, sizeof(resp_bot->mensaje), "%s", texto_plano);
        free(chunk_envio.response);
    }
}

int leer_token(const char *ruta_archivo, char *token, int max_tam) {
    FILE *f = fopen(ruta_archivo, "r");
    if (f == NULL) {
        perror("No se pudo abrir el archivo del token");
        return 0;
    }
    if (fgets(token, max_tam, f) == NULL) {
        fclose(f);
        return 0;
    }
    fclose(f);
    token[strcspn(token, "\r\n")] = '\0';
    return 1;
}

void doomsday(long fecha_actual, char *destino, int max_tam) {
    
    long estreno = 1797562800; 
    long restante = estreno - fecha_actual;

    if (restante > 0) {
        long dias     = restante / 86400;          
        long minutos  = (restante % 86400) / 60;   
        long segundos = restante % 60;             

        snprintf(destino, max_tam,
                 "Faltan %ld dias, %ld minutos y %ld segundos para Avengers: Doomsday",
                 dias, minutos, segundos);
    } else {
        snprintf(destino, max_tam, "Avengers: Doomsday ya se estreno!");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <archivo_token.txt>\n", argv[0]);
        return 1;
    }

    char token[128];
    if (!leer_token(argv[1], token, sizeof(token))) {
        return 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("Error al inicializar libcurl\n");
        return 1;
    }

    long ultimo_update_id = 0;
    char api_url[256];

    printf("Bot iniciado. Esperando mensajes (Ctrl + C para salir)...\n");

    while (1) {
      
        if (ultimo_update_id > 0) {
            snprintf(api_url, sizeof(api_url),"https://api.telegram.org/bot%s/getUpdates?offset=%ld",token, ultimo_update_id + 1);
        } else {
            snprintf(api_url, sizeof(api_url),"https://api.telegram.org/bot%s/getUpdates",token);
        }

        struct memory chunk = {0};
        mensajeTelegram mensaje = {0};

        curl_easy_setopt(curl, CURLOPT_URL, api_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("Error en getUpdates: %d\n", res);
        }
    
        if (chunk.response != NULL) {
            if (rescatarDatos(chunk, &mensaje) == 1) {
                printf("[%ld] %s: %s\n", mensaje.date, mensaje.name, mensaje.mensaje);
                guardar_en_txt(&mensaje);
              
                if (strcasecmp(mensaje.mensaje, "hola") == 0) {
                    char respuesta[256];
                    mensajeTelegram resp_bot = {0};

                    snprintf(respuesta, sizeof(respuesta), "Hola, %s", mensaje.name);
                    enviar_mensaje(curl, token, mensaje.chat_id, respuesta, &resp_bot);
                    guardar_en_txt(&resp_bot);
                    printf("[%ld] %s: %s\n", resp_bot.date, resp_bot.name, resp_bot.mensaje);

                } 
                else {
                  if (strcasecmp(mensaje.mensaje, "chau") == 0) {
                    char respuesta[256];
                    mensajeTelegram resp_bot = {0};
                    snprintf(respuesta, sizeof(respuesta), "Chau %s, que tengas lindo dia", mensaje.name);
                    enviar_mensaje(curl, token, mensaje.chat_id, respuesta, &resp_bot);
                    guardar_en_txt(&resp_bot);
                    printf("[%ld] %s: %s\n", resp_bot.date, resp_bot.name, resp_bot.mensaje);
                }
                else{
                  if (strcasecmp(mensaje.mensaje, "doomsday") == 0) {
                    char respuesta[256];
                    mensajeTelegram resp_bot = {0};
                    doomsday(mensaje.date, respuesta, sizeof(respuesta));
                    enviar_mensaje(curl, token, mensaje.chat_id, respuesta, &resp_bot);
                    guardar_en_txt(&resp_bot);
                    printf("[%ld] %s: %s\n", resp_bot.date, resp_bot.name, resp_bot.mensaje);
                  }
                  else{
                    if(strcasecmp(mensaje.mensaje, "perro") == 0) {
                      char url[256] = "https://dog.ceo/api/breeds/image/random";
                      struct memory respuesta_api = {0};
                      curl_easy_setopt(curl, CURLOPT_URL, url);
                      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
                      curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&respuesta_api);
                      CURLcode res = curl_easy_perform(curl);
                      if (res == CURLE_OK && respuesta_api.response != NULL) {
                        char url_foto[512];
                        buscar_texto(respuesta_api.response, "\"message\":\"", url_foto, sizeof(url_foto));
                        mensajeTelegram resp_bot = {0};
                        enviar_mensaje(curl, token, mensaje.chat_id, url_foto, &resp_bot);
                        guardar_en_txt(&resp_bot);
                        printf("[%ld] %s: %s\n", resp_bot.date, resp_bot.name, resp_bot.mensaje);
                        free(respuesta_api.response);
                    } else {
                        printf("Error al consultar la API (%s): %d\n", url, res);
                    }
                    }
                    
                  }
                }
              }
              
                ultimo_update_id = mensaje.update_id;
            }
            free(chunk.response);
        }

        sleep(2); 
    }

    curl_easy_cleanup(curl);
    return 0;
}
