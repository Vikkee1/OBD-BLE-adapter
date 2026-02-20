/* Includes */
#include "gatt.h"
#include "common.h"

/* Private function declarations */
static int obd_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);

/* Private variables */
/* OBD service */
static const ble_uuid128_t obd_service_svc_uuid =
    BLE_UUID128_INIT(0x21,0x43,0x65,0x87,
                     0x09,0xba,
                     0xdc,0xfe,
                     0xef,0xcd,
                     0xab,0x90,0x78,0x56,0x34,0x12);

static uint8_t obd_chr_val[8] = {0};
static uint16_t obd_chr_val_handle;
static const ble_uuid128_t obd_service_chr_uuid =
    BLE_UUID128_INIT(0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef,
                      0xfe,0xdc,0xba,0x09,0x87,0x65,0x43,0x21);

static uint16_t obd_chr_conn_handle = 0;
static bool obd_chr_conn_handle_inited = false;
static bool obd_ind_status = false;

int test = 0;

/* GATT services table */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /* OBD service */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &obd_service_svc_uuid.u,
        .characteristics = 
            (struct ble_gatt_chr_def[]) {
                {
                .uuid = &obd_service_chr_uuid.u,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .access_cb = obd_chr_access,
                .val_handle = &obd_chr_val_handle},
                {
                    0, /* No more characteristics in this service. */
                }}},

    {
        0, /* No more services. */
    },
};

/* Private functions */
static int obd_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
    /* Local variables */
    int rc;
    static int test = 0;

    /* Handle access events */
    switch (ctxt->op) {

    /* Read characteristic event */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(GATT_TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(GATT_TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

        /* Verify attribute handle */
        if (attr_handle == obd_chr_val_handle) {
            /* Update access buffer value */
            test ++;
            obd_chr_val[0] = test;
            rc = os_mbuf_append(ctxt->om, &obd_chr_val,
                                sizeof(obd_chr_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(
        GATT_TAG,
        "unexpected access operation to obd characteristic, opcode: %d",
        ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op) {

    /* Service register event */
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(GATT_TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    /* Characteristic register event */
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(GATT_TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    /* Descriptor register event */
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(GATT_TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    /* Unknown event */
    default:
        ESP_LOGE(GATT_TAG, "Unknown event");
        assert(0);
        break;
    }
}

/*
 *  GATT server subscribe event callback
 *      1. Update data
 */
void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    ESP_LOGW(GATT_TAG, "%d %d", event->subscribe.attr_handle, obd_chr_val_handle);
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(GATT_TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(GATT_TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

    /* Check attribute handle */
    if (event->subscribe.attr_handle == obd_chr_val_handle) {
        obd_chr_conn_handle = event->subscribe.conn_handle;
        obd_chr_conn_handle_inited = true;
        obd_ind_status = event->subscribe.cur_notify;
        ESP_LOGD(GATT_TAG, "Attribute handle intialized");
    }else{
        ESP_LOGW(GATT_TAG, "Attribute not initialized");
    }
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void) {
    /* Local variables */
    int rc;

    /* GATT service initialization */
    ble_svc_gatt_init();

    /* Update GATT services counter */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    /* Add GATT services */
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

uint16_t get_ble_conn_handle(void){
    return obd_chr_conn_handle;
}

uint16_t get_ble_val_handle(void){
    return obd_chr_val_handle;
}

bool is_connected(void){
    return obd_ind_status && obd_chr_conn_handle_inited;
}