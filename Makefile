# Определите переменные для компилятора и флагов
CXX = g++
CXXFLAGS = -std=c++17 -Wall
LDFLAGS = -lpthread

# Имя целевого исполняемого файла и путь к папке build
TARGET = build/kmi_server
BUILD_DIR = build
CONFIG_FILE = $(BUILD_DIR)/kmi.config

# Список исходных файлов
SRCS = main.cpp config_parser.cpp

# Правило по умолчанию
all: $(CONFIG_FILE) $(TARGET)

# Правило для создания конфигурационного файла
$(CONFIG_FILE):
	@mkdir -p $(BUILD_DIR)
	@echo "[KMI.Settings]" > $(CONFIG_FILE)
	@echo "TimeToExpired=43200" >> $(CONFIG_FILE)
	@echo "[KMI.WebServer]" >> $(CONFIG_FILE)
	@echo "Host=0.0.0.0" >> $(CONFIG_FILE)
	@echo "Port=9912" >> $(CONFIG_FILE)

# Правило для создания исполняемого файла
$(TARGET): $(SRCS) $(CONFIG_FILE)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(LDFLAGS)

# Очистка временных файлов
clean:
	rm -rf $(BUILD_DIR)
