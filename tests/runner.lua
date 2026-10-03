local posix  = require("posix")
local unistd = require("posix.unistd")
local signal = require("posix.signal")
local wait   = require("posix.sys.wait")

local iso_path = arg[1] or "build/kernel.iso"
local expected = arg[2] or "[OK] kfree ejecutado"
local timeout  = 5

local read_fd, write_fd = unistd.pipe()
local pid = unistd.fork()

if pid == 0 then
-- [ PROCESO HIJO : QEMU ]
    unistd.close(read_fd)

    unistd.dup2(write_fd, unistd.STDOUT_FILENO)
    unistd.dup2(write_fd, unistd.STDERR_FILENO)
    unistd.close(write_fd)

    unistd.execp("qemu-system-x86_64", {
        "-M", "q35",
        "-m", "512M",
        "-cdrom", iso_path,
        "-serial", "stdio",
        "-display", "none"
    })
    os.exit(1)
else
    -- [ PROCESO PADRE : TEST RUNNER ]
    unistd.close(write_fd)
    print(string.format("[*] Ejecutando QEMU (PID: %d) buscando: '%s'", pid, expected))

    signal.signal(signal.SIGALRM, function()
        print("\n[-] TEST FAILED: Timeout alcanzado (El kernel se colgó).")
        signal.kill(pid, signal.SIGTERM)
        os.exit(1)
    end)
    unistd.alarm(timeout)

    local passed = false
    local buffer = ""

    -- Bucle de lectura manual sobre el File Descriptor
    while true do
        local chunk = unistd.read(read_fd, 128)
        if not chunk or #chunk == 0 then break end

        buffer = buffer .. chunk

        -- Buscar saltos de línea para procesar línea por línea
        while true do
            local nl_pos = string.find(buffer, "\n")
            if not nl_pos then break end

            local line = string.sub(buffer, 1, nl_pos - 1)
            buffer = string.sub(buffer, nl_pos + 1)

            -- Limpiar retorno de carro (\r) que QEMU suele escupir por UART
            line = string.gsub(line, "\r", "")

            if line ~= "" then
                print("  [QEMU] " .. line)
                if string.find(line, expected, 1, true) then
                    passed = true
                    break
                end
            end
        end
        if passed then break end
    end
    unistd.close(read_fd)
    unistd.alarm(0)
    signal.kill(pid, signal.SIGTERM)
    wait.wait(pid)

    if passed then
        print("\n[+] TEST PASSED: Salida correcta detectada.")
        os.exit(0)
    else
        print("\n[-] TEST FAILED: QEMU cerró sin emitir el log esperado.")
        os.exit(1)
    end
end
