/*
    CREDITS:
    - https://gist.github.com/mmozeiko/5e727f845db182d468a34d524508ad5f
    - https://github.com/kevinmoran/BeginnerDirect3D11/tree/master
*/

// TODO: instead of assert_hr use something like check_hr

#include <d3d11u.h>
#include <w32u.h>

typedef struct window_data
{
    int* is_running;
    int* was_resized;
    w32u_input_state* input_state;
} window_data;

LRESULT window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    window_data* data = (window_data*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    int* is_running = data->is_running;
    int* was_resized = data->was_resized;
    w32u_input_state* input_state = data->input_state;

    LRESULT result = 0;
    switch (msg)
    {
    case WM_CLOSE:
    {
        *is_running = 0;
        result = 0;
    } break;
    case WM_SIZE:
    {
        *was_resized = 1;
    } break;
    case WM_MOUSEWHEEL:
    {
        input_state->mouse.wheel = GET_WHEEL_DELTA_WPARAM(wparam);
    } break;
    case WM_MOUSEMOVE:
    {
        input_state->mouse.x = GET_X_LPARAM(lparam); input_state->mouse.y = GET_Y_LPARAM(lparam);
    } break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    {
        input_state->mouse.lbutton = (msg == WM_LBUTTONDOWN);
    } break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    {
        input_state->mouse.mbutton = (msg == WM_MBUTTONDOWN);
    } break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    {
        input_state->mouse.rbutton = (msg == WM_RBUTTONDOWN);
    } break;
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        input_state->keyboard.key[wparam] = (msg == WM_KEYDOWN);
    } break;
    default:
    {
        result = DefWindowProcA(hwnd, msg, wparam, lparam);
    } break;
    }

    return result;
}

#define curr_input(idx) (idx)
#define next_input(idx) (((idx) + 1) % 2)
#define prev_input(idx) next_input(idx)

void d3d11u_create_device(ID3D11Device** out_device, ID3D11DeviceContext** out_context)
{
    UINT flags = 0;
    #if defined(D3D11U_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    w32u_check_hr(D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, levels, sizeof(levels) / sizeof(levels[0]), D3D11_SDK_VERSION, out_device, 0, out_context));
}

void d3d11u_break_on_errors(ID3D11Device* device, w32u_logger logger)
{
    #define warn_if_not_ok(call) { hr = (call); if (!(SUCCEEDED(hr))) w32u_warn(logger, __FILE__ "(" w32u_stringify(__LINE__) "): " #call " failed"); }

    HRESULT hr = S_OK;
    {
        ID3D11InfoQueue* info = 0;
        warn_if_not_ok(ID3D11Device_QueryInterface(device, &IID_ID3D11InfoQueue, &info));
        if (SUCCEEDED(hr))
        {
            warn_if_not_ok(ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_CORRUPTION, 1));
            warn_if_not_ok(ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_ERROR, 1));
        }
        d3d11u_release(info);
    }
    {
        IDXGIInfoQueue* info = 0;
        warn_if_not_ok(DXGIGetDebugInterface1(0, &IID_IDXGIInfoQueue, &info));
        if (SUCCEEDED(hr))
        {
            warn_if_not_ok(IDXGIInfoQueue_SetBreakOnSeverity(info, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, 1));
            warn_if_not_ok(IDXGIInfoQueue_SetBreakOnSeverity(info, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, 1));
        }
        d3d11u_release(info);
    }

    #undef warn_if_not_ok
}

void d3d11u_create_swap_chain_for_hwnd(ID3D11Device* device, HWND window, w32u_logger logger, IDXGISwapChain1** out_swap_chain)
{
    #define warn_if_not_ok(call) { HRESULT hr = (call); if (!(SUCCEEDED(hr))) w32u_warn(logger, __FILE__ "(" w32u_stringify(__LINE__) "): " #call " failed"); }

    IDXGIDevice* dxgi_device = 0;
    w32u_check_hr(ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, &dxgi_device));
    IDXGIAdapter* dxgi_adapter = 0;
    w32u_check_hr(IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter));
    IDXGIFactory2* factory = 0;
    w32u_check_hr(IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, &factory));

    DXGI_SWAP_CHAIN_DESC1 desc = { 0 };
    desc.Width = 0; // NOTE: get width from output window
    desc.Height = 0; // NOTE: get height from output window
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // NOTE: or DXGI_FORMAT_R8G8B8A8_UNORM_SRGB for sRGB
    desc.Stereo = 0;
    desc.SampleDesc = (DXGI_SAMPLE_DESC){ .Count = 1, .Quality = 0 };
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 0;

    w32u_check_hr(IDXGIFactory2_CreateSwapChainForHwnd(factory, device, window, &desc, 0, 0, out_swap_chain));

    // NOTE: disable Alt+Enter changing monitor resolution to match window size
    warn_if_not_ok(IDXGIFactory_MakeWindowAssociation(factory, window, DXGI_MWA_NO_ALT_ENTER));

    d3d11u_release(factory);
    d3d11u_release(dxgi_adapter);
    d3d11u_release(dxgi_device);

    #undef warn_if_not_ok
}

void d3d11u_get_swap_chain_back_buffer_rtv(IDXGISwapChain1* swap_chain, ID3D11Device* device, ID3D11RenderTargetView** out_back_buffer_rtv)
{
    ID3D11Texture2D* back_buffer = 0;
    w32u_check_hr(IDXGISwapChain1_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, &back_buffer));
    w32u_check_hr(ID3D11Device_CreateRenderTargetView(device, back_buffer, 0, out_back_buffer_rtv));
    d3d11u_release(back_buffer);
}

int main(void)
{
    w32u_logger logger = { 0 };
    logger.file = INVALID_HANDLE_VALUE;
    logger.console_out = INVALID_HANDLE_VALUE;
    logger.console_err = INVALID_HANDLE_VALUE;
    logger.debug = 1;
    logger.level = W32U_LOG_LVL_TRACE;

    w32u_input_state input_buf[2] = { 0 };
    int input_idx = 0;

    w32u_check(w32u_make_dpi_aware());

    const char* class_name = "my_window_class_name";
    w32u_check(w32u_register_window_class(class_name));

    int is_running = 1;
    int was_resized = 0;
    window_data window_data = { 0 };
    window_data.is_running = &is_running;
    window_data.was_resized = &was_resized;
    window_data.input_state = &input_buf[curr_input(input_idx)];

    HWND window = 0;
    w32u_check(w32u_create_window(class_name, "Window", 1280, 720, WS_OVERLAPPEDWINDOW, window_proc, &window_data, &window));

    ID3D11Device* device = 0;
    ID3D11DeviceContext* context = 0;
    IDXGISwapChain1* swap_chain = 0;
    ID3D11RenderTargetView* back_buffer_rtv = 0;

    d3d11u_create_device(&device, &context);
    #if defined(D3D11U_DEBUG)
    d3d11u_break_on_errors(device, logger);
    #endif
    d3d11u_create_swap_chain_for_hwnd(device, window, logger, &swap_chain);
    d3d11u_get_swap_chain_back_buffer_rtv(swap_chain, device, &back_buffer_rtv);

    while (is_running)
    {
        input_idx = next_input(input_idx);
        input_buf[curr_input(input_idx)] = input_buf[prev_input(input_idx)];
        input_buf[curr_input(input_idx)].mouse.wheel = 0;
        window_data.input_state = &input_buf[curr_input(input_idx)];

        {
            MSG msg = { 0 };
            while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }

        if (was_resized)
        {
            ID3D11DeviceContext_ClearState(context);
            d3d11u_release(back_buffer_rtv);
            // TODO: may need to release depth buffer
            w32u_check_hr(IDXGISwapChain1_ResizeBuffers(swap_chain, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0)); // TODO: instead of crash we may just warn
            d3d11u_get_swap_chain_back_buffer_rtv(swap_chain, device, &back_buffer_rtv);
            was_resized = 0;
        }

        {
            int was_pressed = input_buf[prev_input(input_idx)].keyboard.key['T'];
            int is_pressed = input_buf[curr_input(input_idx)].keyboard.key['T'];
            if (!was_pressed && is_pressed)
            {
                w32u_trace(logger, "Just pressed T ...");
            }
        }

        // NOTE: render and present
        {
            float clear_color[] = { 0.1f, 0.2f, 0.6f, 1.0f };
            ID3D11DeviceContext_ClearRenderTargetView(context, back_buffer_rtv, clear_color);
            HRESULT hr = IDXGISwapChain1_Present(swap_chain, 1, 0); // TODO: can fail
        }
    }

    d3d11u_release(back_buffer_rtv);
    d3d11u_release(swap_chain);
    d3d11u_release(context);
    d3d11u_release(device);

    DestroyWindow(window);
    UnregisterClassA(class_name, 0);

    return 0;
}
