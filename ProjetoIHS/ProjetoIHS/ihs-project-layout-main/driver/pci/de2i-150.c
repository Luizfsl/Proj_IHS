#include <linux/init.h>      // Inicialização do módulo do kernel
#include <linux/module.h>    // Suporte para módulos do kernel
#include <linux/fs.h>        // Operações de sistema de arquivos
#include <linux/ioctl.h>     // Suporte para IOCTL
#include <linux/errno.h>     // Definições de códigos de erro
#include <linux/types.h>     // Definições de tipos
#include <linux/cdev.h>      // Suporte para drivers de caractere
#include <linux/uaccess.h>   // Acesso a espaço do usuário
#include <linux/pci.h>       // Manipulação de dispositivos PCI

// Definição de metadados do módulo
/*
	- São Macros que definem informações sobre o módulo, como licença, autor e descrição.
*/
MODULE_LICENSE("GPL"); // Declara a licença sobre o qual o modulo do kernel é distribuído / Permite que o módulo use símbolos exportados apenas para módulos compatíveis com a GPL / Informa ao kernel que o módulo é de código aberto
MODULE_AUTHOR("mfbsouza");// Declara o autor do módulo
MODULE_DESCRIPTION("Simple PCI driver for DE2i-150 dev board");// Declara a descrição do módulo, descreve o propósito do módulo

// Definições de constantes
#define DRIVER_NAME      "my_driver"   	// Nome do driver // Finalidade de identificar o driver no kernel
#define FILE_NAME        "mydev"       	// Nome do arquivo do dispositivo // Arquivo criado no sistema de arquivos(/dev) e permite que os app de espaço do usuário interajam com o driver
#define DRIVER_CLASS     "MyModuleClass"// Classe do dispositivo // Defini-se classe para agrupar dispositivos relacionados no sistema de arquivos
#define MY_PCI_VENDOR_ID  0x1172        // ID do fornecedor PCI
#define MY_PCI_DEVICE_ID  0x0004        // ID do dispositivo PCI

// Prototipação das funções
/*
	- As funções são STATIC para restringir seu escopo ao arquivo que estão definidas;
		--> Isso evita que outras partes do código (em outros arquivos) acessem essas funções direteamente, o que promove o encapsulamento e reduz o risco de conflitos de nomes.
	- Declarar como Static garante que elas não sejam visíveis fora do arquivo de origem;
*/
static int  __init my_init (void);   // Função de inicialização do driver
static void __exit my_exit (void);   // Função de finalização do driver
static int  my_open(struct inode*, struct file*); // Função de abertura do dispositivo
static int  my_close(struct inode*, struct file*); // Função de fechamento do dispositivo
static ssize_t my_read(struct file*, char __user*, size_t, loff_t*); // Leitura // Permite que o driver leia dados do dispositivo e os copie para o espaço do usuário
static ssize_t my_write(struct file*, const char __user*, size_t, loff_t*); // Escrita // Permite que o driver escreva dados no dispositivo a partir do espaço do usuário
static long int my_ioctl(struct file*, unsigned int, unsigned long); // Input/Output Control // Permite que o driver receba comandos específicos do usuário para controlar o dispositivo, como: Configurar o dispositivo, Selecionar periféricos p/ leitura ou escrita e enviar comandos para o HW.
static int  __init my_pci_probe(struct pci_dev *dev, const struct pci_device_id *id); // Detecção PCI // Chamada automaticamete pelo kernel quando um dispos. PCI compatível com o driver é detectado // Realiza a inicialização do dispositivo PCI, como habilitação, mapeamento de memória.
static void __exit my_pci_remove(struct pci_dev *dev); // Remoção do dispositivo PCI // Realiza a limpeza e a liberação de recursos alocados durante a inicialização do dispositivo PCI.

// Definição dos dispositivos PCI compatíveis com o driver e registra as informações no Kernel
static struct pci_device_id pci_ids[] = { // É uma estrutura que contém uma lista de dispositivos PCI compatíveis com o driver
    {PCI_DEVICE(MY_PCI_VENDOR_ID, MY_PCI_DEVICE_ID), }, //Cada entrada especifica o dispositivo suportado
    {0, } // Terminação da lista
};
MODULE_DEVICE_TABLE(pci, pci_ids); // Macro que registra a tabela de dispositivos PCI no kernel

// Estrutura de operações do dispositivo de caractere
/*
	- É uma estrutura que contém ponteiros para funções que implementam as operações do driver de caractere.
	- Essas funções são chamadas pelo kernel quando um aplicativo tenta interagir com o dispositivo.
*/
static struct file_operations fops = {
    .owner = THIS_MODULE, 		// Define o proprietário da estrutura como o módulo atual // Garante que o módulo não seja descarregado enquanto o disp. estiver em uso.
    .read = my_read,	 		// Ponteiro para a função de leitura
    .write = my_write,   		// Ponteiro para a função de escrita
    .unlocked_ioctl = my_ioctl, // Ponteiro para a função de IOCTL
    .open = my_open,			// Ponteiro para a função de abertura
    .release = my_close			// Ponteiro para a função de fechamento
};

// Estrutura de operações do driver PCI
/*
	- Usado para registrar e gerenciar o driver PCI no kernel.
	- É uma estrutura que contém ponteiros para funções que implementam as operações do driver PCI.
	- Essas funções são chamadas pelo kernel quando um dispositivo PCI compatível é detectado ou removido.
*/
static struct pci_driver pci_ops = {
    .name = DRIVER_NAME, 	// Nome do driver PCI
    .id_table = pci_ids,	// Tabela de IDs de dispositivos PCI compatíveis
    .probe = my_pci_probe,	// Ponteiro para a função de detecção do dispositivo PCI
    .remove = my_pci_remove	// Ponteiro para a função de remoção do dispositivo PCI
};

// Variáveis para registro do dispositivo Para SISTEMA DE ARQUIVOS
static dev_t my_device_nbr; // Número do dispositivo // Núm. composto por dois valores: Major(Identifica o driver no kernel) e Minor(Identifica o dispositivo específico gerenciado pelo driver) // Usado para criar o arquivo de dispositivo no SISTEMA DE ARQUIVOS
static struct class* my_class; // Classe do dispositivo // Agrupa dispositivos relacionados no sistema de arquivos // Permite que o kernel saiba como interagir com o dispositivo
static struct cdev my_device; // Estrutura de caractere que representa o dispositivo //Contém as informações sobre o disp. e as operações // Registra o disp. de caractere no kernel e associa as operações dos dispotisito (definidas em file_operations) ao disp. de caractere 

// Variáveis para mapeamento de memória
/*
	- Essas variáveis são usadas para armazenar os ponteiros para as regiões de memória mapeadas do dispositivo PCI.
	- Essas regiões de memória são usadas para comunicação entre o driver e o hardware.
*/
static void __iomem* bar0_mmio = NULL; // Base do mapeamento do BAR0 (Base Address Register 0) // Representa a região de memória mapeada do dispositivo PCI // Permite que o driver acesse à memória do dispositivo PCI // É usado como base para calcular os endereços de registradores ou áreas específicas do dispositivo.
static void __iomem* read_pointer = NULL; // Ponteiro de leitura configurado para acessar uma região específica do dispositivo PCI // É inicializado com base no mapeamento de bar0_mmio e um deslocamento (offset) para o registrador ou área de leitura // Permite que o driver leia dados do dispositivo PCI 
static void __iomem* write_pointer = NULL; // Ponteiro de escrita configurado para acessar uma região específica do dispositivo PCI // É inicializado com base no mapeamento de bar0_mmio e um deslocamento (offset) para o registrador ou área de escrita // Permite que o driver escreva dados no dispositivo PCI // Está configurado dinamicamente a função my_ioctl com base no comando recebido do usuário.

// Definição de nomes dos periféricos para fins de depuração no dmesg
static const char* peripheral[] = {
    "switches",     // Interruptores
    "p_buttons",    // Botões físicos
    "display_l",    // Display esquerdo
    "display_r",    // Display direito
    "green_leds",   // LEDs verdes
    "red_leds"      // LEDs vermelhos
};

// Enumeração de índices para identificar os periféricos conectados ao disp. PCI
enum perf_names_idx {
    IDX_SWITCH = 0,  // Índice dos interruptores
    IDX_PBUTTONS,    // Índice dos botões físicos
    IDX_DISPLAYL,    // Índice do display esquerdo
    IDX_DISPLAYR,    // Índice do display direito
    IDX_GREENLED,    // Índice dos LEDs verdes
    IDX_REDLED       // Índice dos LEDs vermelhos
};

// Variáveis para controlar os nomes de periféricos usados na escrita e leitura
static int wr_name_idx = IDX_DISPLAYR; // Índice do periférico de escrita padrão (display direito)
static int rd_name_idx = IDX_SWITCH;   // Índice do periférico de leitura padrão (interruptores)


// Função de inicialização do driver
// Chamada automaticamente pelo kernel quando o módulo é carregado
static int __init my_init(void) // Principal função é configurar e registrar os componentes necessários para que o driver funcione corretamente
{
	/*
		Registra o driver PCI.
		Configura o dispositivo de caractere.
		Cria o arquivo de dispositivo no sistema de arquivos.
		Trata erros de forma robusta, garantindo que recursos sejam liberados em caso de falha.
	*/
    printk("my_driver: loaded to the kernel\n");
    
    // Registra o driver PCI
    if (pci_register_driver(&pci_ops) < 0) { 
		/*
			- Registra o driver PCI no kernel usando a estrutura pci_ops que é uma estrutura de operações do driver PCI.
			- Essa função é responsável por associar o driver a dispositivos PCI compatíveis.
		*/
        printk("my_driver: PCI driver registration failed\n");// 
        return -EAGAIN; // Significa que o driver não pôde ser registrado e o kernel deve tentar novamente mais tarde.
    }
    
    // Aloca um número de dispositivo(Major e Minor) para o driver // Sistema de arquivos
    if (alloc_chrdev_region(&my_device_nbr, 0, 1, DRIVER_NAME) < 0) {
        printk("my_driver: device number could not be allocated!\n");
        return -EAGAIN;
    }
    printk("my_driver: device number %d was registered!\n", MAJOR(my_device_nbr));
    
    // Cria uma classe de dispositivo
    if ((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) { // Cria uma classe de dispositivo no sistema de arquivos
        printk("my_driver: device class could not be created!\n");
        goto ClassError; // Se falhar a criação vai para o rótulo de erro
    }
    
    // Inicializa o dispositivo(my_device) como um dispositivo de caractere // Associa o dispositivo a um conjunto de operações de arquivo
    cdev_init(&my_device, &fops); 
    
    // Cria um arquivo de dispositivo no Sistema de Arquivos // Permite que aplicativos de espaço do usuário interajam com o driver 
    if (device_create(my_class, NULL, my_device_nbr, NULL, FILE_NAME) == NULL) {
        printk("my_driver: cannot create device file!\n");
        goto FileError; // falhou vai para FileError
    }
    
    // Adiciona o dispositivo de caracter ao kernel
    if (cdev_add(&my_device, my_device_nbr, 1) == -1){
        printk("my_driver: registering of device to kernel failed!\n");
        goto AddError;
    }
    return 0;

// Tratamento de erro
AddError:
    device_destroy(my_class, my_device_nbr); // Remove o arquivo de dispositivo do sistema de arquivos
FileError:
    class_destroy(my_class); // Remove a classe de dispositivo do sistema de arquivos
ClassError: 
    unregister_chrdev(my_device_nbr, DRIVER_NAME);
    pci_unregister_driver(&pci_ops); // Desregistra o número do dispositivo e o driver PCI
    return -EAGAIN;
}

// Função de finalização do driver
static void __exit my_exit(void)
{
	// Sua principal responsabilidade é liberar todos os recursos alocados durante a inicialização do driver (my_init) e garantir que o sistema volte ao estado anterior ao carregamento do módulo.
    cdev_del(&my_device);
    device_destroy(my_class, my_device_nbr);
    class_destroy(my_class);
    unregister_chrdev(my_device_nbr, DRIVER_NAME);
    pci_unregister_driver(&pci_ops);
    printk("my_driver: goodbye kernel!\n");
}

// Função chamada quando o dispositivo é aberto
// Essa função é chamada quando um processo tenta abrir o dispositivo
/*
	- struct inode* inode = Representa o nó do dispositivo no sistema de arquivos.
	- struct file* filp = Representa o ponteiro para o arquivo aberto.
		--> Contém informações sobre o arquivo aberto, como flags, posição do ponteiro de leitura/escrita e outras informações relacionadas ao arquivo.
*/
static int my_open(struct inode* inode, struct file* filp)
{
    printk("my_driver: open was called\n"); // Mensagem de depuração informando que o dispositivo foi aberto
    return 0; // Retorna 0 indicando sucesso
}

// Função chamada quando o dispositivo é fechado
static int my_close(struct inode* inode, struct file* filp)
{
	// Essa função é chamada quando um processo tenta fechar o dispositivo
    printk("my_driver: close was called\n"); // Mensagem de depuração informando que o dispositivo foi fechado
    return 0; // Retorna 0 indicando sucesso
}

// Função chamada quando um processo tenta ler do dispositivo
static ssize_t my_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
	/*
		- struct file* filp = Representa o ponteiro para o arquivo aberto.
		- char __user* buf = Ponteiro para o buffer de leitura do usuário. Os dados lidos serão copiados para esse buffer.
		- size_t count = Número máximo de bytes a serem lidos.
		- loff_t* f_pos = Ponteiro para a posição atual no arquivo.
	*/
    ssize_t retval = 0; // Armazena o número de bytes lidos
    int to_cpy = 0; // Armazena o número de bytes a serem copiados
    static unsigned int temp_read = 0; // Armazena temporariamente o valor lido do dispositivo

    // Verifica se o ponteiro de leitura está configurado
    if (read_pointer == NULL) {
        printk("my_driver: trying to read to a device region not set yet\n");
        return -ECANCELED;
    }

    // Lê um valor de 32 bits do dispositivo no endereço apontado por read_pointer
    temp_read = ioread32(read_pointer);
    printk("my_driver: read 0x%X from the %s\n", temp_read, peripheral[rd_name_idx]); // Exibe o valor lido e o periferico correspondente

    // Determina a quantidade de bytes a copiar para o usuário
    to_cpy = (count <= sizeof(temp_read)) ? count : sizeof(temp_read);

    // Usa a função copy_to_user para copiar os dados do buffer temporário para o espaço do usuário
	// copy_to_user retorna o número de bytes que não puderam ser copiados
	// retval armazena o número de bytes que foram copiados com sucesso
    retval = to_cpy - copy_to_user(buf, &temp_read, to_cpy);

    return retval;
}

// Função chamada quando um processo tenta escrever no dispositivo
static ssize_t my_write(struct file* filp, const char __user* buf, size_t count, loff_t* f_pos)
{
	/*
		- struct file* filp = Representa o ponteiro para o arquivo aberto.
		- const char __user* buf = Ponteiro para o buffer de escrita do usuário. Os dados a serem escritos serão lidos desse buffer.
		- size_t count = Número máximo de bytes a serem escritos.
		- loff_t* f_pos = Ponteiro para a posição atual no arquivo.
	*/
	//  Essa função é responsável por copiar dados do espaço do usuário para o dispositivo e realizar a escrita no hardware.
	// O ponteiro de escrita é configurado com base no comando IOCTL recebido anteriormente.
	// O ponteiro de escrita é usado para determinar onde os dados devem ser escritos no dispositivo PCI.

    ssize_t retval = 0;
    int to_cpy = 0;
    static unsigned int temp_write = 0; // Armazena temporariamente os dados copiados do espaço do usuário.

    // Verifica se o ponteiro de escrita está configurado
    if (write_pointer == NULL) {
        printk("my_driver: trying to write to a device region not set yet\n");
        return -ECANCELED;
    }

    // Determina a quantidade de bytes a copiar do usuário
    to_cpy = (count <= sizeof(temp_write)) ? count : sizeof(temp_write);

    // Copia os dados do espaço do usuário
	// retval armazena o número de bytes que foram copiados com sucesso
	// copy_from_user retorna o número de bytes que não puderam ser copiados
    retval = to_cpy - copy_from_user(&temp_write, buf, to_cpy);

    // Escreve os dados no dispositivo
    iowrite32(temp_write, write_pointer); // Usa a função iowrite32 para escrever os dados no endereço apontado por write_pointer
    printk("my_writer: wrote 0x%X to the %s\n", temp_write, peripheral[wr_name_idx]);

    return retval;
}

// Função chamada ao receber um comando IOCTL
static long int my_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
	/*
		- struct file* filp = Representa o ponteiro para o arquivo
		- unsigned int cmd = Comando IOCTL recebido do usuário.
		- unsigned long arg = Argumento adicional passado pelo usuário (geralmente um ponteiro para dados ou uma estrutura).
	*/
    switch(cmd) {
        case RD_SWITCHES:
            // Configura o ponteiro de leitura para os interruptores
            read_pointer = bar0_mmio + 0xC0A0; // TODO: atualizar offset
            rd_name_idx = IDX_SWITCH;
            break;
        case RD_PBUTTONS:
            // Configura o ponteiro de leitura para os botões físicos
            read_pointer = bar0_mmio + 0xC080; // TODO: atualizar offset
            rd_name_idx = IDX_PBUTTONS;
            break;
        case WR_L_DISPLAY:
            // Configura o ponteiro de escrita para o display esquerdo
            write_pointer = bar0_mmio + 0xC000; // TODO: atualizar offset
            wr_name_idx = IDX_DISPLAYL;
            break;
        case WR_R_DISPLAY:
            // Configura o ponteiro de escrita para o display direito
            write_pointer = bar0_mmio + 0xC060; // TODO: atualizar offset
            wr_name_idx = IDX_DISPLAYR;
            break;
        case WR_RED_LEDS:
            // Configura o ponteiro de escrita para os LEDs vermelhos
            write_pointer = bar0_mmio + 0xC0C0; // TODO: atualizar offset
            wr_name_idx = IDX_REDLED;
            break;
        case WR_GREEN_LEDS:
            // Configura o ponteiro de escrita para os LEDs verdes
            write_pointer = bar0_mmio + 0xC0E0; // TODO: atualizar offset
            wr_name_idx = IDX_GREENLED;
            break;
        default:
            // Comando IOCTL desconhecido
            printk("my_driver: unknown ioctl command: 0x%X\n", cmd);
    }
    return 0;
}

// Função de detecção do dispositivo PCI
static int __init my_pci_probe(struct pci_dev *dev, const struct pci_device_id *id) 
	// Chamada quando um dispositivo PCI compatível é detectado
	// Essa função é responsável por habilitar o dispositivo PCI, ler informações do espaço de configuração PCI e mapear a memória do dispositivo para o espaço do kernel.
	/*
		- struct pci_dev* dev = Representa o dispositivo PCI detectado.
		- const struct pci_device_id* id = Representa o ID do dispositivo PCI compatível.
	*/
{
    unsigned short vendor, device;
    unsigned char rev;
    unsigned int bar_value;
    unsigned long bar_len;

    // Habilita o dispositivo PCI para que o driver possa acessá-lo
    if (pci_enable_device(dev) < 0) {
        printk("my_driver: Could not enable the PCI device!\n");
        return -EBUSY;
    }
    printk("my_driver: PCI device enabled\n");

    // Lê informações do espaço de configuração PCI com a fins de depuração
    pci_read_config_word(dev, PCI_VENDOR_ID, &vendor); // Informa o ID do fornecedor
    pci_read_config_word(dev, PCI_DEVICE_ID, &device); // Informa o ID do dispositivo
    pci_read_config_byte(dev, PCI_REVISION_ID, &rev); // Informa a versão do dispositivo
    printk("my_driver: PCI device - Vendor 0x%X Device 0x%X Rev 0x%X\n", vendor, device, rev);

    // Lê informações sobre o BAR0 do dispositivo PCI
    pci_read_config_dword(dev, 0x10, &bar_value); // Lê o valor do BAR0 (Base Address Register 0) // O BAR0 é usado para mapear a memória do dispositivo PCI para o espaço de endereço do kernel
    bar_len = pci_resource_len(dev, 0); // Lê o comprimento da região de memória mapeada pelo BAR0 // Essa função retorna o tamanho da região de memória associada ao BAR0
    printk("my_driver: PCI device - BAR0 => 0x%X with length of %ld Kbytes\n", bar_value, bar_len/1024);

    // Marca a região PCI BAR0 como reservada para este driver
	// A reserva da região de memória BAR0 é necessária para garantir que o driver tenha acesso exclusivo à região de memória mapeada pelo dispositivo PCI.
    if (pci_request_region(dev, 0, DRIVER_NAME) != 0) {
        printk("my_driver: PCI Error - PCI BAR0 region already in use!\n");
        pci_disable_device(dev);
        return -EBUSY;
    }

    // Mapeia o espaço de endereço físico BAR0 para espaço virtual
    bar0_mmio = pci_iomap(dev, 0, bar_len);

    // Inicializa ponteiros de leitura e escrita padrão
	// É necessário para que o driver possa acessar diretamente os registradores ou áreas de memória do dispositivo PCI.
    write_pointer = bar0_mmio + 0xC000; // TODO: atualizar offset
    read_pointer  = bar0_mmio + 0xC080; // TODO: atualizar offset

    return 0;
}

// Função de remoção do dispositivo PCI
static void __exit my_pci_remove(struct pci_dev *dev)
{
	// Aqui serve para liberar os recursos alocados durante a inicialização do dispositivo PCI e desabilitar o dispositivo.
    read_pointer = NULL;
    write_pointer = NULL;

    // Remove o mapeamento de IO feito na função probe
	// O mapeamento de IO é a associação entre o espaço de endereço físico do dispositivo PCI e o espaço de endereço virtual do kernel.
    pci_iounmap(dev, bar0_mmio);

    // Desabilita o dispositivo PCI
    pci_disable_device(dev);

    // Libera a região PCI BAR0 reservada
    pci_release_region(dev, 0);

    printk("my_driver: PCI Device - Disabled and BAR0 Released");
}

// Define as funções de inicialização e saída do módulo
module_init(my_init); // Função chamada na inicialização do módulo
module_exit(my_exit); // Função chamada na remoção do módulo